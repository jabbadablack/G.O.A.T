#pragma once

#include <Core/Application/AgentArchetype.h>
#include <Core/Application/GuardWatch.h>
#include <Core/Frontend/DecisionCursor.h>

#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/AgentStateMachine.h>
#include <GOAT/Domain/DecisionProgram.h>
#include <GOAT/Domain/DirectorProfile.h>

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

    //! Everything that differs between two agents authored the same way.
    //!
    //! What they have in common -- their compiled trees and what those are called -- lives on
    //! the archetype they share, so what is left here is one agent's own position and state.
    //! Every tree is remembered by its slot in that archetype rather than by name, which is why
    //! a switch is a byte and an interrupted stack is eight of them.
    struct AgentRecord final
    {
        AZ_CLASS_ALLOCATOR(AgentRecord, AZ::SystemAllocator);

        //! Handle other systems refer to this agent by.
        AgentId m_id;
        //! The entity this agent drives.
        AZ::EntityId m_entity;

        //! Everything this kind of agent shares with the others authored like it.
        AZStd::shared_ptr<const AgentArchetype> m_archetype;
        //! The tree it is running, as a slot in that archetype.
        TreeSlot m_tree = 0;
        //! Trees this agent interrupted, innermost last. Bounded, because an agent that pushes
        //! without ever popping is looping rather than layering.
        AZStd::fixed_vector<TreeSlot, MaxTreeStackDepth> m_treeStack;
        //! The compiled tree m_tree names, held directly because every tick dereferences it.
        //! The archetype owns it, so this is a cached lookup rather than a second owner.
        const DecisionProgram* m_program = nullptr;
        //! Where this agent is inside that tree.
        DecisionCursor m_cursor;
        //! The action this agent is running, and the plan it came from.
        AgentStateMachine m_machine;
        //! Wakes the agent only when a scope its tree guards on has changed.
        GuardWatch m_observer;

        //! Earliest time a cooldown that turned this agent's last walk away expires. Together
        //! with the observer's dirty flag it is the whole wake condition: a tree that found no
        //! work is not walked again until a slot it guards on changes or this comes due.
        float m_wakeAt = 0.0f;

        //! Which pacing band this agent belongs to, which is its level of detail.
        AZ::u8 m_band = 0;

        //! A tree change asked for while the agent was mid tick, applied at the top of the next
        //! one. Switching in place would rewrite the program and cursor that Tick is holding
        //! references into, and ctx:SetTree is reachable from a behaviour running inside Tick.
        TreeSlot m_pendingTree = InvalidTreeSlot;
        TreeSwitchKind m_pendingSwitch = TreeSwitchKind::None;
        //! Priority of whoever asked. A higher one replaces a command still waiting; a lower one
        //! arriving after is dropped rather than queued, because queueing it would land it on the
        //! next window and undo the winner one tick later.
        AZ::u8 m_pendingPriority = SelfSwitchPriority;


        //! Slot of a tree this entity declared it may run, or InvalidTreeSlot. A switch to
        //! anything else is refused, so no order can put an agent somewhere its author did not
        //! sanction -- and the repertoire is the archetype's list rather than a copy per agent.
        TreeSlot FindTree(const AZ::Name& treeName) const
        {
            return m_archetype != nullptr ? m_archetype->FindTree(treeName) : InvalidTreeSlot;
        }

        //! What the tree this agent is running is called.
        AZ::Name GetTreeName() const
        {
            return m_archetype != nullptr ? m_archetype->GetName(m_tree) : AZ::Name{};
        }
    };
} // namespace GOAT
