#include <Navigation/SpatialChecks.h>

#include <Navigation/NavigationTarget.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT_Navigation
{
    namespace
    {
        //! How close counts as being at a location when a node names no tolerance.
        constexpr float DefaultArrivalTolerance = 0.5f;

        //! This agent's outstanding path request, kept in the scratch.
        PathRequestId& Outstanding(const GOAT::ActionContext& context)
        {
            return *reinterpret_cast<PathRequestId*>(context.m_scratch->data());
        }

        static_assert(sizeof(PathRequestId) <= AZStd::tuple_size<GOAT::ActionScratch>::value,
            "A path request id does not fit in the action scratch");
    } // namespace

    AZ::Name IsAtLocationAction::GetName() const
    {
        return AZ_NAME_LITERAL("is_at_location");
    }

    void IsAtLocationAction::Begin([[maybe_unused]] const GOAT::ActionContext& context)
    {
    }

    GOAT::ActionResult IsAtLocationAction::Step(const GOAT::ActionContext& context, [[maybe_unused]] float deltaTime)
    {
        AZ::Vector3 target = AZ::Vector3::CreateZero();
        if (!ReadActionTarget(context, target))
        {
            return GOAT::ActionResult::Failure;
        }

        const AZ::Vector3 position = ReadActionPosition(context);

        const float tolerance = context.m_request->m_tolerance > 0.0f ? context.m_request->m_tolerance : DefaultArrivalTolerance;
        AZ_Assert(tolerance > 0.0f, "An arrival tolerance must be positive");

        return position.GetDistance(target) <= tolerance ? GOAT::ActionResult::Success : GOAT::ActionResult::Failure;
    }

    void IsAtLocationAction::End([[maybe_unused]] const GOAT::ActionContext& context)
    {
    }

    DoesPathExistAction::DoesPathExistAction(NavigationService& service)
        : m_service(service)
    {
    }

    AZ::Name DoesPathExistAction::GetName() const
    {
        return AZ_NAME_LITERAL("does_path_exist");
    }

    void DoesPathExistAction::Begin(const GOAT::ActionContext& context)
    {
        PathRequestId& outstanding = Outstanding(context);
        outstanding = InvalidPathRequestId;

        AZ::Vector3 target = AZ::Vector3::CreateZero();
        if (!ReadActionTarget(context, target))
        {
            return;
        }

        const AZ::Vector3 from = ReadActionPosition(context);
        outstanding = m_service.RequestPath(from, target);
        if (outstanding == InvalidPathRequestId)
        {
            // See MoveToAction: still building is transient, nothing bound is a setup mistake.
            AZ_Warning("GOAT", m_service.HasNavigationMesh(),
                "does_path_exist cannot query because no navigation mesh is bound to GOAT; "
                "add a GOAT Nav Mesh component beside the Recast navigation mesh");

            AZLOG(GoatNav, "does_path_exist for agent %u waits: the navigation mesh is not built yet",
                context.m_agent.GetIndex());
        }
    }

    GOAT::ActionResult DoesPathExistAction::Step(const GOAT::ActionContext& context, [[maybe_unused]] float deltaTime)
    {
        PathRequestId& outstanding = Outstanding(context);
        if (outstanding == InvalidPathRequestId)
        {
            return GOAT::ActionResult::Failure;
        }

        // The path itself is thrown away: only whether one exists is being asked.
        AZStd::vector<AZ::Vector3> path;
        if (!m_service.TakePath(outstanding, path))
        {
            const PathStatus status = m_service.GetStatus(outstanding);
            return status == PathStatus::Pending || status == PathStatus::Running ? GOAT::ActionResult::Running
                                                                                  : GOAT::ActionResult::Failure;
        }

        outstanding = InvalidPathRequestId;
        return path.empty() ? GOAT::ActionResult::Failure : GOAT::ActionResult::Success;
    }

    void DoesPathExistAction::End(const GOAT::ActionContext& context)
    {
        PathRequestId& outstanding = Outstanding(context);
        m_service.CancelRequest(outstanding);
        outstanding = InvalidPathRequestId;
    }
} // namespace GOAT_Navigation
