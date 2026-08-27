#include <Navigation/MoveToAction.h>

#include <Navigation/NavigationTarget.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT_Navigation
{
    namespace
    {
        //! Speed used when a move node names none, in metres per second.
        AZ_CVAR(float, goat_navDefaultSpeed, 3.0f, nullptr, AZ::ConsoleFunctorFlags::Null,
            "Movement speed GOAT uses when a move_to node does not name one");

        //! How close counts as reaching a waypoint when a move node names no tolerance.
        constexpr float DefaultWaypointTolerance = 0.25f;

        //! What a move is doing right now. Kept in the agent's scratch.
        enum class Phase : AZ::u8
        {
            Querying, //!< Waiting for the path query to answer.
            Following //!< Walking a path held in a pooled slot.
        };

        //! Everything one agent's move needs between steps.
        struct MoveState
        {
            PathRequestId m_request = InvalidPathRequestId;
            PathSlot m_slot = InvalidPathSlot;
            AZ::u32 m_waypoint = 0;
            Phase m_phase = Phase::Querying;
        };

        static_assert(sizeof(MoveState) <= AZStd::tuple_size<GOAT::ActionScratch>::value,
            "Move state does not fit in the action scratch");

        //! This agent's move state, which the state machine zeroed before Begin.
        MoveState& State(const GOAT::ActionContext& context)
        {
            return *reinterpret_cast<MoveState*>(context.m_scratch->data());
        }

    } // namespace

    MoveToAction::MoveToAction(NavigationService& service, PathPool& paths, const NavigationKeys& keys)
        : m_service(service)
        , m_paths(paths)
        , m_keys(keys)
    {
        AZ_Assert(m_keys.IsValid(), "move_to needs its blackboard variables declared before it runs");
    }

    AZ::Name MoveToAction::GetName() const
    {
        return AZ_NAME_LITERAL("move_to");
    }

    void MoveToAction::Begin(const GOAT::ActionContext& context)
    {
        AZ_Assert(context.m_request != nullptr, "An action always begins with a request");
        AZ_Assert(context.m_blackboard != nullptr, "An action always begins with a blackboard");

        MoveState& state = State(context);
        state = MoveState{};

        AZ::Vector3 target = AZ::Vector3::CreateZero();
        if (!ReadActionTarget(context, target))
        {
            return;
        }

        const AZ::Vector3 from = ReadActionPosition(context);
        state.m_request = m_service.RequestPath(from, target);
        if (state.m_request == InvalidPathRequestId)
        {
            // A mesh that is merely still building is the normal case for the first tick or two
            // of a level, and the leaf simply fails and is retried. Nothing bound at all is a
            // setup mistake that will never fix itself, so only that one is worth a warning.
            AZ_Warning("GOAT", m_service.HasNavigationMesh(),
                "move_to cannot path because no navigation mesh is bound to GOAT; "
                "add a GOAT Nav Mesh component beside the Recast navigation mesh");

            AZLOG(GoatNav, "move_to for agent %u waits: the navigation mesh is not built yet",
                context.m_agent.GetIndex());
        }
    }

    GOAT::ActionResult MoveToAction::CollectPath(const GOAT::ActionContext& context)
    {
        MoveState& state = State(context);
        AZ_Assert(state.m_phase == Phase::Querying, "Collecting a path outside the querying phase");

        AZStd::vector<AZ::Vector3> path;
        if (!m_service.TakePath(state.m_request, path))
        {
            // Still out. A cancelled or unknown request reports NotFound rather than Pending.
            const PathStatus status = m_service.GetStatus(state.m_request);
            return status == PathStatus::Pending || status == PathStatus::Running ? GOAT::ActionResult::Running
                                                                                 : GOAT::ActionResult::Failure;
        }

        state.m_request = InvalidPathRequestId;
        if (path.empty())
        {
            AZLOG(GoatNav, "move_to found no path for agent %u", context.m_agent.GetIndex());
            return GOAT::ActionResult::Failure;
        }

        state.m_slot = m_paths.Acquire();
        AZStd::vector<AZ::Vector3>* stored = m_paths.Find(state.m_slot);
        AZ_Assert(stored != nullptr, "A freshly acquired path slot must be findable");
        if (stored == nullptr)
        {
            return GOAT::ActionResult::Failure;
        }

        *stored = AZStd::move(path);
        state.m_waypoint = 0;
        state.m_phase = Phase::Following;
        return GOAT::ActionResult::Running;
    }

    GOAT::ActionResult MoveToAction::FollowPath(const GOAT::ActionContext& context, float deltaTime)
    {
        MoveState& state = State(context);
        AZ_Assert(state.m_phase == Phase::Following, "Following a path outside the following phase");

        const AZStd::vector<AZ::Vector3>* path = m_paths.Find(state.m_slot);
        AZ_Assert(path != nullptr, "A following move must hold a borrowed path slot");
        if (path == nullptr || path->empty())
        {
            return GOAT::ActionResult::Failure;
        }

        const AZ::Vector3 position = ReadActionPosition(context);

        const float tolerance = context.m_request->m_tolerance > 0.0f ? context.m_request->m_tolerance : DefaultWaypointTolerance;

        // Skip every waypoint already reached, so a fast agent does not stall on a short leg.
        while (state.m_waypoint < path->size() && position.GetDistance((*path)[state.m_waypoint]) <= tolerance)
        {
            ++state.m_waypoint;
        }

        if (state.m_waypoint >= path->size())
        {
            Publish(context, position, 0.0f);
            return GOAT::ActionResult::Success;
        }

        const AZ::Vector3 waypoint = (*path)[state.m_waypoint];

        float remaining = position.GetDistance(waypoint);
        for (size_t i = state.m_waypoint + 1; i < path->size(); ++i)
        {
            remaining += (*path)[i].GetDistance((*path)[i - 1]);
        }

        Publish(context, waypoint, remaining);

        const bool* steer = context.m_blackboard->Find<bool>(m_keys.m_steer, context.m_agent);
        if (steer != nullptr && !*steer)
        {
            // The project drives the entity; this verb only reports where it should go.
            return GOAT::ActionResult::Running;
        }

        const float speed = context.m_request->m_amount > 0.0f ? context.m_request->m_amount : goat_navDefaultSpeed;
        AZ_Assert(speed > 0.0f, "Movement speed must be positive");

        const AZ::Vector3 toWaypoint = waypoint - position;
        const float distance = toWaypoint.GetLength();
        const float travel = speed * deltaTime;

        // Never overshoot: a long frame would otherwise push the agent past the waypoint.
        const AZ::Vector3 next = travel >= distance ? waypoint : position + toWaypoint.GetNormalized() * travel;
        AZ::TransformBus::Event(context.m_entity, &AZ::TransformInterface::SetWorldTranslation, next);

        return GOAT::ActionResult::Running;
    }

    GOAT::ActionResult MoveToAction::Step(const GOAT::ActionContext& context, float deltaTime)
    {
        MoveState& state = State(context);

        if (state.m_phase == Phase::Querying)
        {
            if (state.m_request == InvalidPathRequestId)
            {
                // Begin could not queue a query, so there is nothing to wait for.
                return GOAT::ActionResult::Failure;
            }
            return CollectPath(context);
        }

        return FollowPath(context, deltaTime);
    }

    void MoveToAction::Publish(const GOAT::ActionContext& context, const AZ::Vector3& waypoint, float remaining) const
    {
        AZ_Assert(m_keys.IsValid(), "Publishing navigation progress needs declared blackboard variables");

        context.m_blackboard->Set<AZ::Vector3>(m_keys.m_waypoint, waypoint, context.m_agent);
        context.m_blackboard->Set<float>(m_keys.m_remaining, remaining, context.m_agent);
    }

    void MoveToAction::End(const GOAT::ActionContext& context)
    {
        MoveState& state = State(context);

        // Aborting mid-query must not leave the service holding work nobody will collect.
        m_service.CancelRequest(state.m_request);
        m_paths.Release(state.m_slot);

        state = MoveState{};
    }
} // namespace GOAT_Navigation
