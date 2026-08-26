#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Task/TaskExecutor.h>
#include <AzCore/Task/TaskGraph.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/Time/ITime.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/parallel/condition_variable.h>
#include <AzCore/std/parallel/mutex.h>
#include <AzCore/std/parallel/shared_mutex.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <RecastNavigation/RecastNavigationMeshBus.h>
#include <RecastNavigation/RecastSmartPointer.h>

namespace GOAT_Navigation
{
    //! Identifies one path request for the lifetime of that request.
    using PathRequestId = AZ::u32;

    //! Value meaning "no request".
    inline constexpr PathRequestId InvalidPathRequestId = 0;

    //! How a finished request turned out.
    enum class PathStatus : AZ::u8
    {
        Pending,  //!< Queued, not yet handed to a worker.
        Running,  //!< Handed to a worker; must not be erased until it reports back.
        Ready,    //!< A path was found.
        NotFound, //!< The query ran but no path exists.
        Cancelled
    };

    //! Answers path queries off the main thread.
    //!
    //! RecastNavigation hands out a single dtNavMeshQuery behind one exclusive mutex, and a
    //! dtNavMeshQuery carries mutable node pools, so sharing it would serialise every query.
    //! This service borrows only the dtNavMesh and gives each worker its own query object.
    //!
    //! The mesh itself is still mutated by Recast's own tile tasks, so reads are guarded by a
    //! shared_mutex owned here whose write window is driven by the nav mesh notifications.
    class NavigationService final
        : private RecastNavigation::RecastNavigationMeshNotificationBus::Handler
    {
    public:
        NavigationService();
        ~NavigationService();

        //! Binds to a navigation mesh entity. Safe to call again to rebind.
        void SetNavigationMesh(AZ::EntityId navMeshEntity);

        //! Releases the mesh binding and cancels everything in flight.
        void ClearNavigationMesh();

        //! True once a mesh is bound and its worker queries are usable.
        bool IsReady() const;

        //! Queues a path query. Returns InvalidPathRequestId when no mesh is bound.
        PathRequestId RequestPath(const AZ::Vector3& from, const AZ::Vector3& to);

        //! Reports how a request is doing without consuming it.
        PathStatus GetStatus(PathRequestId request) const;

        //! Moves a finished path out of the service. Returns false while it is still pending.
        bool TakePath(PathRequestId request, AZStd::vector<AZ::Vector3>& outPath);

        //! Abandons a request. Safe for an id that already completed.
        void CancelRequest(PathRequestId request);

        //! Retires finished tasks and recycles their storage. Call once per frame.
        void Update();

        //! How many requests are queued or running, for console output.
        size_t GetPendingCount() const;

    private:
        //! One in flight query and its result.
        struct Request final
        {
            PathRequestId m_id = InvalidPathRequestId;
            AZ::Vector3 m_from = AZ::Vector3::CreateZero();
            AZ::Vector3 m_to = AZ::Vector3::CreateZero();
            AZStd::vector<AZ::Vector3> m_path;
            PathStatus m_status = PathStatus::Pending;
        };

        //! A worker's private query object, so node pools are never shared.
        struct Worker final
        {
            RecastNavigation::RecastPointer<dtNavMeshQuery> m_query;
            bool m_initialised = false;
        };

        ////////////////////////////////////////////////////////////////////////
        // RecastNavigation::RecastNavigationMeshNotificationBus
        void OnNavigationMeshUpdated(AZ::EntityId navigationMeshEntity) override;
        void OnNavigationMeshBeganRecalculating(AZ::EntityId navigationMeshEntity) override;
        ////////////////////////////////////////////////////////////////////////

        //! Points every worker query at the current mesh. Takes the write lock.
        void RebindWorkers();

        //! Drops requests cancelled while their worker was running. No tasks may be in flight.
        void ReapCancelled();

        //! Blocks until no worker is running. Must be called before anything a worker reads is torn down.
        void WaitForInFlight();

        //! Reports one worker finished. The last one wakes whoever is waiting on the batch.
        void FinishTask();

        //! Destroys the finished batch's graph, once it is safe to.
        void RetireBatch();

        //! Re-binds when a mesh should be usable but is not, so a missed notification does not
        //! leave path queries failing for the rest of the level.
        void RecoverBinding();

        //! Runs one query and stores the result by id. Takes no pointer into the request table,
        //! because RequestPath may reallocate it while this runs on a worker thread.
        void RunQuery(PathRequestId id, Worker& worker);

        //! The work one worker does, wrapped by RunQuery so the in flight count always falls.
        void RunQueryAndStore(PathRequestId id, Worker& worker);

        //! The query itself, on a worker's own objects, under a shared read lock.
        void RunQueryImpl(
            const AZ::Vector3& from,
            const AZ::Vector3& to,
            Worker& worker,
            AZStd::vector<AZ::Vector3>& outPath,
            PathStatus& outStatus) const;

        //! Submits everything queued that is not yet running.
        void SubmitPending();

        AZ::EntityId m_navMeshEntity;
        //! Kept alive so the mesh outlives a rebuild that swaps Recast's own object.
        AZStd::shared_ptr<RecastNavigation::NavMeshQuery> m_navObject;
        //! Snapshot taken under Recast's lock; only read afterwards.
        dtNavMesh* m_navMesh = nullptr;

        //! Guards m_navMesh reads against the rebuild window. Not Recast's mutex, which is exclusive.
        mutable AZStd::shared_mutex m_meshLock;
        //! Guards the request table.
        mutable AZStd::mutex m_requestLock;

        AZStd::vector<Worker> m_workers;
        AZStd::vector<Request> m_requests;
        PathRequestId m_nextRequestId = 1;

        //! Sized from goat_pathQueryThreads at construction; TaskExecutor takes its count there.
        AZStd::unique_ptr<AZ::TaskExecutor> m_executor;

        //! One graph per batch, never reused. AZ::TaskGraph sets its submitted flag *after*
        //! handing the tasks to the executor, so a batch that finishes during that call clears
        //! the flag first and has it set again permanently -- after which Reset and AddTask
        //! both assert. Short path queries hit that race routinely, so nothing is reused.
        AZStd::unique_ptr<AZ::TaskGraph> m_taskGraph;

        //! Not a resubmit gate -- the in flight count is that. This exists only so the graph
        //! can be destroyed safely: the count falls inside a task body, which is before the
        //! executor releases that task, and the graph must outlive every release.
        AZStd::unique_ptr<AZ::TaskGraphEvent> m_taskGraphEvent;
        AZ::TaskDescriptor m_taskDescriptor{ "Path query", "GOAT Navigation" };

        //! How many workers are still running, which is the only thing gating a new batch.
        //! Counted here rather than read from a TaskGraphEvent so completion does not depend
        //! on the graph's own state, which the race above corrupts.
        mutable AZStd::mutex m_flightLock;
        AZStd::condition_variable m_flightIdle;
        size_t m_tasksInFlight = 0;

        //! True between a mesh rebuild starting and finishing, when reads must not run.
        //! Atomic because Recast raises those notifications from whichever thread drives the
        //! rebuild, while Update reads this on the main thread.
        AZStd::atomic<bool> m_recalculating{ false };
        //! When that window opened, so a rebuild that never reports finishing cannot stall paths.
        AZStd::atomic<AZ::TimeMs> m_recalculatingSince{ AZ::TimeMs{ 0 } };
    };
} // namespace GOAT_Navigation
