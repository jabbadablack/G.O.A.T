#pragma once

#include <Core/Application/AgentArchetype.h>
#include <Core/Application/GuardWatch.h>

#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/AgentProgram.h>
#include <GOAT/Domain/AgentStateMachine.h>
#include <GOAT/Domain/DirectorProfile.h>
#include <GOAT/Interfaces/IDecisionBackend.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/fixed_vector.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace GOAT
{
    //! How deep an agent may interrupt itself before the stack is treated as a bug.
    inline constexpr size_t MaxTreeStackDepth = 8;

    //! How much state one agent may carry for its backend.
    inline constexpr size_t MaxBrainState = 72;

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
    //! What they have in common -- their compiled programs and what those are called -- lives on
    //! the archetype they share, so what is left here is one agent's own position and state.
    struct AgentRecord final
    {
        AZ_CLASS_ALLOCATOR(AgentRecord, AZ::SystemAllocator);

        //! Handle other systems refer to this agent by.
        AgentId m_id;
        //! The entity this agent drives.
        AZ::EntityId m_entity;

        //! Everything this kind of agent shares with the others authored like it.
        AZStd::shared_ptr<const AgentArchetype> m_archetype;
        //! The program it is running. The archetype owns it.
        const AgentProgram* m_program = nullptr;

        //! The action this agent is running, and the plan it came from.
        AgentStateMachine m_machine;
        //! Wakes the agent only when a scope its program guards on has changed.
        GuardWatch m_observer;

        //! Where this agent is inside its program, in whatever shape its backend chose.
        alignas(8) AZStd::array<AZ::u8, MaxBrainState> m_brainState{};

        //! Seconds until this agent is worth deciding for again.
        float m_wakeIn = 0.0f;
        //! Seconds since its backend was last called, so time still runs while it is left alone.
        float m_elapsed = 0.0f;

        //! The program it is running, as a slot in that archetype.
        TreeSlot m_tree = 0;
        //! Programs this agent interrupted, innermost last.
        AZStd::fixed_vector<TreeSlot, MaxTreeStackDepth> m_treeStack;

        //! Which pacing band this agent belongs to, which is its level of detail.
        AZ::u8 m_band = 0;

        //! A tree change asked for while the agent was mid tick, applied at the top of the next
        //! one. Switching in place would rewrite the program Tick is holding a reference into.
        TreeSlot m_pendingTree = InvalidTreeSlot;
        TreeSwitchKind m_pendingSwitch = TreeSwitchKind::None;
        //! Priority of whoever asked. A higher one replaces a command still waiting.
        AZ::u8 m_pendingPriority = SelfSwitchPriority;

        //! What this agent's program is run by.
        IDecisionBackend* GetBackend() const { return m_program != nullptr ? m_program->m_backend : nullptr; }

        //! The block this agent's backend keeps its state in.
        BrainState GetState() { return BrainState(m_brainState.data(), m_brainState.size()); }

        //! Slot of a program this entity declared it may run, or InvalidTreeSlot.
        TreeSlot FindTree(const AZ::Name& treeName) const
        {
            return m_archetype != nullptr ? m_archetype->FindTree(treeName) : InvalidTreeSlot;
        }

        //! What the program this agent is running is called.
        AZ::Name GetTreeName() const
        {
            return m_archetype != nullptr ? m_archetype->GetName(m_tree) : AZ::Name{};
        }
    };
} // namespace GOAT
