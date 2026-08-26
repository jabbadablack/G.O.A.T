#pragma once

#include <GOAT/Domain/ActionPlan.h>
#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/Guard.h>
#include <GOAT/Domain/Intent.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>

namespace GOAT
{
    class IBlackboardSystem;
    class INodeScripting;

    //! Everything a backend may reach while planning for one agent.
    struct PlanContext final
    {
        //! The agent being planned for.
        AgentId m_agent;
        //! The entity the agent drives.
        AZ::EntityId m_entity;
        //! Shared data, the only thing a backend and the tree have in common.
        IBlackboardSystem* m_blackboard = nullptr;
        //! User defined control flow, when any is installed. Backends do not use this.
        INodeScripting* m_scripting = nullptr;
    };

    //! Turns an intent from the tree into a sequence of action states the agent can run.
    //! Behavior tree leaves, planners, directors and bark systems all plug in here.
    class IBackend
    {
    public:
        AZ_RTTI(IBackend, IBackendTypeId);

        virtual ~IBackend() = default;

        //! Name this backend is registered under and referenced by from Lua.
        virtual AZ::Name GetName() const = 0;

        //! Produces a plan for one intent. Returns false when this backend cannot satisfy it.
        virtual bool Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan) = 0;

        //! Reports the conditions that invalidate the plan while it runs.
        virtual void CollectGuards(
            [[maybe_unused]] const PlanContext& context,
            [[maybe_unused]] const ActionPlan& plan,
            [[maybe_unused]] GuardList& outGuards) const
        {
        }

        //! Releases any per agent state held for this agent.
        virtual void Release([[maybe_unused]] const PlanContext& context)
        {
        }
    };
} // namespace GOAT
