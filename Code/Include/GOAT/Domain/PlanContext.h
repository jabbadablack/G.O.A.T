#pragma once

#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/PlanStore.h>

#include <AzCore/Component/EntityId.h>

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
        //! Where a backend puts the steps it produces. A plan is a span into this rather than a
        //! buffer of its own, which is what lets a plan be any length.
        PlanStore* m_planStore = nullptr;
    };
} // namespace GOAT
