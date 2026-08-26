#include <Navigation/NavigationService.h>

#include <AzCore/Console/IConsole.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/std/parallel/lock.h>

#include <RecastNavigation/RecastHelpers.h>

#include <DetourNavMeshQuery.h>

namespace GOAT_Navigation
{
    namespace
    {
        //! Worker threads used for path queries. One private dtNavMeshQuery is kept per worker.
        AZ_CVAR(AZ::u32, goat_pathQueryThreads, 2, nullptr, AZ::ConsoleFunctorFlags::Null,
            "Number of threads GOAT uses to answer navigation path queries");

        //! Requests started per frame, so a burst of agents cannot flood the task graph.
        AZ_CVAR(AZ::u32, goat_pathQueryBudget, 16, nullptr, AZ::ConsoleFunctorFlags::Null,
            "Maximum navigation path queries GOAT submits in one frame");

        //! Search nodes each worker query may use. Recast's own mesh controller uses the same value.
        constexpr int MaxSearchNodes = 2048;
        //! Longest polygon path a single query may return.
        constexpr int MaxPathPolygons = 256;
        //! Longest waypoint path a single query may return.
        constexpr int MaxPathPoints = 256;
        //! How far from a given position a walkable polygon is looked for.
        constexpr float PolygonSearchExtents = 3.0f;

        //! How long a mesh rebuild may run before queries stop waiting for it to report finishing.
        constexpr AZ::TimeMs RecalculationGrace{ 5000 };
    } // namespace

    NavigationService::NavigationService()
    {
        const AZ::u32 workerCount = AZStd::max<AZ::u32>(goat_pathQueryThreads, 1);
        m_workers.resize(workerCount);
        m_executor = AZStd::make_unique<AZ::TaskExecutor>(workerCount);

        AZ_Assert(!m_workers.empty(), "A navigation service must have at least one worker");
    }

    NavigationService::~NavigationService()
    {
        ClearNavigationMesh();
    }

    void NavigationService::SetNavigationMesh(AZ::EntityId navMeshEntity)
    {
        AZ_Assert(navMeshEntity.IsValid(), "A navigation mesh must be bound to a valid entity");
        if (!navMeshEntity.IsValid())
        {
            return;
        }

        ClearNavigationMesh();

        m_navMeshEntity = navMeshEntity;
        RecastNavigation::RecastNavigationMeshNotificationBus::Handler::BusConnect(navMeshEntity);
        RebindWorkers();

        AZ_Warning("GOAT", m_navMesh != nullptr,
            "Navigation mesh entity %s has no built mesh yet; paths will fail until it is built",
            navMeshEntity.ToString().c_str());
    }

    void NavigationService::WaitForInFlight()
    {
        {
            AZStd::unique_lock<AZStd::mutex> lock(m_flightLock);
            m_flightIdle.wait(lock, [this] { return m_tasksInFlight == 0; });
            AZ_Assert(m_tasksInFlight == 0, "Waiting must leave no worker running");
        }

        RetireBatch();
    }

    void NavigationService::RetireBatch()
    {
        // The wait event signals only once the executor has released every task, which is
        // strictly later than the last task body running, so this is what makes destroying
        // the graph safe. It returns at once when the batch has already finished.
        if (m_taskGraphEvent != nullptr)
        {
            m_taskGraphEvent->Wait();
        }

        m_taskGraphEvent.reset();
        m_taskGraph.reset();

        AZ_Assert(m_taskGraph == nullptr, "Retiring a batch must leave no graph behind");
    }

    void NavigationService::FinishTask()
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_flightLock);

        AZ_Assert(m_tasksInFlight > 0, "A worker finished a batch that was not counted as running");
        if (m_tasksInFlight > 0 && --m_tasksInFlight == 0)
        {
            m_flightIdle.notify_all();
        }
    }

    void NavigationService::ClearNavigationMesh()
    {
        RecastNavigation::RecastNavigationMeshNotificationBus::Handler::BusDisconnect();

        // No lock is held here on purpose: a worker still writing its result needs m_requestLock.
        WaitForInFlight();

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_requestLock);
            for (Request& request : m_requests)
            {
                request.m_status = PathStatus::Cancelled;
            }
        }

        // Nothing may read the mesh pointer past this point.
        AZStd::unique_lock<AZStd::shared_mutex> writeLock(m_meshLock);
        m_navMesh = nullptr;
        m_navObject.reset();
        m_navMeshEntity = AZ::EntityId{};
        m_recalculating = false;
        for (Worker& worker : m_workers)
        {
            worker.m_initialised = false;
        }
    }

    bool NavigationService::IsReady() const
    {
        AZStd::shared_lock<AZStd::shared_mutex> readLock(m_meshLock);
        return m_navMesh != nullptr;
    }

    void NavigationService::RebindWorkers()
    {
        AZStd::shared_ptr<RecastNavigation::NavMeshQuery> navObject;
        RecastNavigation::RecastNavigationMeshRequestBus::EventResult(
            navObject, m_navMeshEntity, &RecastNavigation::RecastNavigationMeshRequests::GetNavigationObject);

        AZStd::unique_lock<AZStd::shared_mutex> writeLock(m_meshLock);

        m_navObject = navObject;
        m_navMesh = nullptr;
        for (Worker& worker : m_workers)
        {
            worker.m_initialised = false;
        }

        if (navObject == nullptr)
        {
            return;
        }

        // Recast's own mutex is only taken to read the mesh pointer out; queries below never take it.
        {
            RecastNavigation::NavMeshQuery::LockGuard recastLock(*navObject);
            m_navMesh = recastLock.GetNavMesh();
        }

        if (m_navMesh == nullptr)
        {
            return;
        }

        for (Worker& worker : m_workers)
        {
            if (worker.m_query == nullptr)
            {
                worker.m_query.reset(dtAllocNavMeshQuery());
            }

            AZ_Assert(worker.m_query != nullptr, "Detour failed to allocate a navigation query");
            if (worker.m_query == nullptr)
            {
                continue;
            }

            // init takes a const dtNavMesh*, so a worker query only ever reads the mesh.
            worker.m_initialised = dtStatusSucceed(worker.m_query->init(m_navMesh, MaxSearchNodes));
            AZ_Warning("GOAT", worker.m_initialised, "A navigation worker query failed to bind to the mesh");
        }
    }

    void NavigationService::OnNavigationMeshBeganRecalculating([[maybe_unused]] AZ::EntityId navigationMeshEntity)
    {
        AZ_Assert(navigationMeshEntity == m_navMeshEntity,
            "A navigation notification arrived for an entity this service is not bound to");

        m_recalculating = true;
        m_recalculatingSince = AZ::GetElapsedTimeMs();

        // Recast is about to add and remove tiles, so stop every reader until it is done.
        AZStd::unique_lock<AZStd::shared_mutex> writeLock(m_meshLock);
        m_navMesh = nullptr;
        for (Worker& worker : m_workers)
        {
            worker.m_initialised = false;
        }
    }

    void NavigationService::OnNavigationMeshUpdated([[maybe_unused]] AZ::EntityId navigationMeshEntity)
    {
        AZ_Assert(navigationMeshEntity == m_navMeshEntity,
            "A navigation notification arrived for an entity this service is not bound to");

        m_recalculating = false;

        // The mesh object itself may have been replaced, so re-fetch and re-init rather than reuse.
        RebindWorkers();
    }

    PathRequestId NavigationService::RequestPath(const AZ::Vector3& from, const AZ::Vector3& to)
    {
        if (!IsReady())
        {
            return InvalidPathRequestId;
        }

        AZStd::lock_guard<AZStd::mutex> lock(m_requestLock);

        Request request;
        request.m_id = m_nextRequestId++;
        request.m_from = from;
        request.m_to = to;
        request.m_status = PathStatus::Pending;

        AZ_Assert(request.m_id != InvalidPathRequestId, "A path request id must never collide with the null id");

        m_requests.push_back(AZStd::move(request));
        return m_requests.back().m_id;
    }

    PathStatus NavigationService::GetStatus(PathRequestId request) const
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_requestLock);
        for (const Request& entry : m_requests)
        {
            if (entry.m_id == request)
            {
                return entry.m_status;
            }
        }
        return PathStatus::Cancelled;
    }

    bool NavigationService::TakePath(PathRequestId request, AZStd::vector<AZ::Vector3>& outPath)
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_requestLock);

        for (size_t i = 0; i < m_requests.size(); ++i)
        {
            if (m_requests[i].m_id != request)
            {
                continue;
            }

            if (m_requests[i].m_status == PathStatus::Pending || m_requests[i].m_status == PathStatus::Running)
            {
                return false;
            }

            outPath = AZStd::move(m_requests[i].m_path);
            m_requests.erase(m_requests.begin() + i);
            return true;
        }

        return false;
    }

    void NavigationService::CancelRequest(PathRequestId request)
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_requestLock);
        for (size_t i = 0; i < m_requests.size(); ++i)
        {
            if (m_requests[i].m_id != request)
            {
                continue;
            }

            // A worker may be writing to this entry, so mark it and let Update reap it instead.
            if (m_requests[i].m_status == PathStatus::Running)
            {
                m_requests[i].m_status = PathStatus::Cancelled;
                return;
            }

            m_requests.erase(m_requests.begin() + i);
            return;
        }
    }

    size_t NavigationService::GetPendingCount() const
    {
        AZStd::lock_guard<AZStd::mutex> lock(m_requestLock);
        return m_requests.size();
    }

    void NavigationService::RunQuery(PathRequestId id, Worker& worker)
    {
        // The count must fall on every path out of the work below, so the work is nested.
        RunQueryAndStore(id, worker);
        FinishTask();
    }

    void NavigationService::RunQueryAndStore(PathRequestId id, Worker& worker)
    {
        // Read the endpoints back out by id: a Running request is never erased, so it is still here.
        AZ::Vector3 from = AZ::Vector3::CreateZero();
        AZ::Vector3 to = AZ::Vector3::CreateZero();
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_requestLock);
            const Request* found = nullptr;
            for (const Request& request : m_requests)
            {
                if (request.m_id == id)
                {
                    found = &request;
                    break;
                }
            }

            AZ_Assert(found != nullptr, "A running path request must still be in the request table");
            if (found == nullptr)
            {
                return;
            }

            from = found->m_from;
            to = found->m_to;
        }

        AZStd::vector<AZ::Vector3> path;
        PathStatus status = PathStatus::NotFound;
        RunQueryImpl(from, to, worker, path, status);

        // Store by id under the lock. If the request was cancelled meanwhile the lookup fails and
        // the result is simply dropped, which is why the task never holds a pointer to it.
        AZStd::lock_guard<AZStd::mutex> lock(m_requestLock);
        for (Request& request : m_requests)
        {
            if (request.m_id != id)
            {
                continue;
            }

            AZ_Assert(request.m_status == PathStatus::Running || request.m_status == PathStatus::Cancelled,
                "A worker finished a request that was not handed to it");

            // Cancelled while this ran: keep it cancelled and let Update reap it.
            if (request.m_status == PathStatus::Cancelled)
            {
                return;
            }

            request.m_path = AZStd::move(path);
            request.m_status = status;
            return;
        }
    }

    void NavigationService::RunQueryImpl(
        const AZ::Vector3& from, const AZ::Vector3& to, Worker& worker,
        AZStd::vector<AZ::Vector3>& outPath, PathStatus& outStatus) const
    {
        // A shared lock, so queries run concurrently but never during a mesh rebuild.
        AZStd::shared_lock<AZStd::shared_mutex> readLock(m_meshLock);

        outStatus = PathStatus::NotFound;
        if (m_navMesh == nullptr || !worker.m_initialised || worker.m_query == nullptr)
        {
            return;
        }

        // Detour works in +Y up; O3DE is +Z up.
        const RecastNavigation::RecastVector3 start = RecastNavigation::RecastVector3::CreateFromVector3SwapYZ(from);
        const RecastNavigation::RecastVector3 end = RecastNavigation::RecastVector3::CreateFromVector3SwapYZ(to);
        const float extents[3] = { PolygonSearchExtents, PolygonSearchExtents, PolygonSearchExtents };

        // Each worker owns its filter; dtQueryFilter is not shareable across concurrent queries.
        const dtQueryFilter filter;

        dtPolyRef startPoly = 0;
        dtPolyRef endPoly = 0;
        RecastNavigation::RecastVector3 nearestStart;
        RecastNavigation::RecastVector3 nearestEnd;

        worker.m_query->findNearestPoly(start.m_xyz, extents, &filter, &startPoly, nearestStart.m_xyz);
        worker.m_query->findNearestPoly(end.m_xyz, extents, &filter, &endPoly, nearestEnd.m_xyz);

        if (startPoly == 0 || endPoly == 0)
        {
            return;
        }

        dtPolyRef polygons[MaxPathPolygons] = {};
        int polygonCount = 0;
        worker.m_query->findPath(
            startPoly, endPoly, nearestStart.m_xyz, nearestEnd.m_xyz, &filter, polygons, &polygonCount, MaxPathPolygons);

        if (polygonCount <= 0)
        {
            return;
        }

        float points[MaxPathPoints * 3] = {};
        int pointCount = 0;
        worker.m_query->findStraightPath(
            nearestStart.m_xyz, nearestEnd.m_xyz, polygons, polygonCount, points, nullptr, nullptr, &pointCount,
            MaxPathPoints);

        if (pointCount <= 0)
        {
            return;
        }

        outPath.clear();
        outPath.reserve(static_cast<size_t>(pointCount));
        for (int i = 0; i < pointCount; ++i)
        {
            const auto point = RecastNavigation::RecastVector3::CreateFromFloatValuesWithoutAxisSwapping(&points[i * 3]);
            outPath.push_back(point.AsVector3WithZup());
        }

        AZ_Assert(!outPath.empty(), "A ready path must contain at least one waypoint");
        outStatus = PathStatus::Ready;
    }

    void NavigationService::SubmitPending()
    {
        AZStd::vector<PathRequestId> toRun;
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_requestLock);
            for (Request& request : m_requests)
            {
                if (request.m_status != PathStatus::Pending)
                {
                    continue;
                }
                request.m_status = PathStatus::Running;
                toRun.push_back(request.m_id);
                if (toRun.size() >= goat_pathQueryBudget)
                {
                    break;
                }
            }
        }

        if (toRun.empty())
        {
            return;
        }

        AZ_Assert(!m_workers.empty(), "Submitting a query with no workers configured");

        // A brand new graph every batch. See the member's comment: a reused one latches its
        // submitted flag when a batch completes during SubmitOnExecutor.
        RetireBatch();
        m_taskGraph = AZStd::make_unique<AZ::TaskGraph>("GOAT navigation queries");
        m_taskGraphEvent = AZStd::make_unique<AZ::TaskGraphEvent>("GOAT navigation queries");

        {
            AZStd::lock_guard<AZStd::mutex> lock(m_flightLock);
            AZ_Assert(m_tasksInFlight == 0, "A batch was submitted while another was still running");
            m_tasksInFlight = toRun.size();
        }

        for (size_t i = 0; i < toRun.size(); ++i)
        {
            // m_workers is sized once at construction, so this pointer stays valid. Only the id
            // is captured because a task lambda may hold at most 56 bytes.
            const PathRequestId id = toRun[i];
            Worker* worker = &m_workers[i % m_workers.size()];
            m_taskGraph->AddTask(m_taskDescriptor,
                [this, id, worker]()
                {
                    RunQuery(id, *worker);
                });
        }

        m_taskGraph->SubmitOnExecutor(*m_executor, m_taskGraphEvent.get());
    }

    void NavigationService::ReapCancelled()
    {
        // Only ever called with no tasks in flight, so no worker can be holding one of these.
        AZStd::lock_guard<AZStd::mutex> lock(m_requestLock);

        for (size_t i = m_requests.size(); i > 0; --i)
        {
            const Request& request = m_requests[i - 1];
            AZ_Assert(request.m_status != PathStatus::Running,
                "A request is still marked running after its task graph completed");

            if (request.m_status == PathStatus::Cancelled)
            {
                m_requests.erase(m_requests.begin() + (i - 1));
            }
        }
    }

    void NavigationService::RecoverBinding()
    {
        if (!m_navMeshEntity.IsValid())
        {
            return;
        }

        {
            AZStd::shared_lock<AZStd::shared_mutex> readLock(m_meshLock);
            if (m_navMesh != nullptr)
            {
                return;
            }
        }

        // Inside a rebuild, having no mesh is correct and reads must keep waiting. Past the
        // grace period it means the finishing notification never arrived, and continuing to
        // wait would fail every path query for the rest of the level.
        const bool recalculating = m_recalculating;
        if (recalculating && AZ::GetElapsedTimeMs() - m_recalculatingSince.load() < RecalculationGrace)
        {
            return;
        }

        AZ_Warning("GOAT", !recalculating,
            "Navigation mesh entity %s never reported finishing its rebuild; re-binding anyway",
            m_navMeshEntity.ToString().c_str());

        m_recalculating = false;
        RebindWorkers();
    }

    void NavigationService::Update()
    {
        {
            AZStd::lock_guard<AZStd::mutex> lock(m_flightLock);
            if (m_tasksInFlight != 0)
            {
                return;
            }
        }

        RecoverBinding();
        ReapCancelled();
        SubmitPending();
    }
} // namespace GOAT_Navigation
