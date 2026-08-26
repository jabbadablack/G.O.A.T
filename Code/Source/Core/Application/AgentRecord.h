#pragma once

#include <Core/Application/AgentObserver.h>
#include <Core/Frontend/DecisionCursor.h>

#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/AgentStateMachine.h>
#include <GOAT/Domain/DecisionProgram.h>
#include <GOAT/Domain/Intent.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Time/ITime.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace GOAT
{
    //! Everything the runtime keeps for one agent.
    //! Records are held behind a unique_ptr so their address stays put while the
    //! registry's dense storage compacts around them.
    struct AgentRecord final
    {
        AZ_CLASS_ALLOCATOR(AgentRecord, AZ::SystemAllocator);

        //! Handle other systems refer to this agent by.
        AgentId m_id;
        //! The entity this agent drives.
        AZ::EntityId m_entity;

        //! The compiled tree this agent runs, shared with every agent using the same tree.
        AZStd::shared_ptr<const DecisionProgram> m_program;
        //! Where this agent is inside that tree.
        DecisionCursor m_cursor;
        //! The action this agent is running, and the plan it came from.
        AgentStateMachine m_machine;
        //! Wakes the agent only when a blackboard slot its tree guards on changes.
        AgentObserver m_observer;

        //! The intent currently being satisfied, kept so a re-plan knows what it was for.
        Intent m_intent;

        //! Which pacing band this agent belongs to, which is its level of detail.
        size_t m_band = 0;

        //! Scratch reused each tick when collecting due services.
        AZStd::vector<AZ::u32> m_dueServices;
    };
} // namespace GOAT
