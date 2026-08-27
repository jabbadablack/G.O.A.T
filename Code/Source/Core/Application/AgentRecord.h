#pragma once

#include <Core/Application/AgentObserver.h>
#include <Core/Frontend/DecisionCursor.h>

#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/AgentStateMachine.h>
#include <GOAT/Domain/DecisionProgram.h>
#include <GOAT/Domain/DirectorProfile.h>
#include <GOAT/Domain/Intent.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Time/ITime.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace GOAT
{
    //! How deep an agent may interrupt itself before the stack is treated as a bug.
    inline constexpr size_t MaxTreeStackDepth = 8;

    //! What a pending tree change should do when the agent next ticks.
    enum class TreeSwitchKind : AZ::u8
    {
        None,   //!< Nothing asked for.
        Set,    //!< Replace the tree and forget what was interrupted.
        Push,   //!< Interrupt, remembering what to come back to.
        Pop     //!< Return to whatever was interrupted.
    };

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
        //! Which tree that is, so a switch can be reported and a push can be returned from.
        AZ::Name m_treeName;
        //! Trees this agent interrupted, innermost last. Bounded, because an agent that pushes
        //! without ever popping is looping rather than layering.
        AZStd::fixed_vector<AZ::Name, MaxTreeStackDepth> m_treeStack;
        //! Where this agent is inside that tree.
        DecisionCursor m_cursor;
        //! The action this agent is running, and the plan it came from.
        AgentStateMachine m_machine;
        //! Wakes the agent only when a blackboard slot its tree guards on changes.
        AgentObserver m_observer;

        //! The intent currently being satisfied, kept so a re-plan knows what it was for.
        Intent m_intent;

        //! The trees this entity declared it may run, including the one it starts in. A switch
        //! to anything else is refused, so no order can put an agent somewhere its author did
        //! not sanction. Set once at registration and read on every switch request.
        AZStd::vector<AZ::Name> m_repertoire;

        //! Which pacing band this agent belongs to, which is its level of detail.
        size_t m_band = 0;

        //! A tree change asked for while the agent was mid tick, applied at the top of the next
        //! one. Switching in place would rewrite the program and cursor that Tick is holding
        //! references into, and ctx:SetTree is reachable from a behaviour running inside Tick.
        AZ::Name m_pendingTree;
        TreeSwitchKind m_pendingSwitch = TreeSwitchKind::None;
        //! Priority of whoever asked. A higher one replaces a command still waiting; a lower one
        //! arriving after is dropped rather than queued, because queueing it would land it on the
        //! next window and undo the winner one tick later.
        AZ::u8 m_pendingPriority = SelfSwitchPriority;

        //! Scratch reused each tick when collecting due services.
        AZStd::vector<AZ::u32> m_dueServices;

        //! True when a tree is one this entity declared it may run.
        bool MayRun(const AZ::Name& treeName) const
        {
            return AZStd::find(m_repertoire.begin(), m_repertoire.end(), treeName) != m_repertoire.end();
        }
    };
} // namespace GOAT
