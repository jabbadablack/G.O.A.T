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
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace GOAT
{
    //! How deep an agent may interrupt itself before the stack is treated as a bug.
    inline constexpr size_t MaxTreeStackDepth = 8;

    //! How deep one program may nest another before the chain is treated as a bug.
    inline constexpr size_t MaxNestDepth = 4;

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
        //! Sized by the archetype for the deepest chain of nesting any of its programs reaches.
        AZStd::unique_ptr<AZ::u8[]> m_brain;
        AZ::u16 m_brainBytes = 0;
        //! How much of it is spoken for, so a nested backend takes the next block and gives it back.
        AZ::u16 m_brainUsed = 0;

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

        //! The block this agent's own backend keeps its state in.
        BrainState GetState() { return BrainState(m_brain.get(), m_brainBytes); }

        //! Sizes the block for a program and hands its own backend the first part of it.
        //! The archetype's total is the floor, so switching programs never reallocates.
        void ResetBrain(const AgentProgram& program)
        {
            const size_t own = AlignState(program.m_backend != nullptr ? program.m_backend->GetStateSize() : 0);
            const size_t declared = m_archetype != nullptr ? m_archetype->GetStateBytes() : 0;
            const size_t needed = AZStd::max(AZStd::max(program.m_stateBytes, declared), own);
            AZ_Assert(needed <= 0xFFFFu, "A program asking for %zu bytes of brain state is a bug in that total", needed);

            if (m_brain == nullptr || m_brainBytes < needed)
            {
                m_brainBytes = static_cast<AZ::u16>(needed);
                m_brain = AZStd::unique_ptr<AZ::u8[]>(new AZ::u8[needed]{});
            }
            else
            {
                AZStd::fill_n(m_brain.get(), m_brainBytes, AZ::u8{ 0 });
            }

            m_brainUsed = static_cast<AZ::u16>(own);
        }

        //! Takes the next unused block for a nested backend, or an empty span when there is none.
        BrainState BorrowState(size_t bytes)
        {
            const size_t taken = AlignState(bytes);
            if (m_brain == nullptr || m_brainUsed + taken > m_brainBytes)
            {
                return BrainState();
            }

            AZ::u8* block = m_brain.get() + m_brainUsed;
            m_brainUsed = static_cast<AZ::u16>(m_brainUsed + taken);
            return BrainState(block, bytes);
        }

        //! Gives the innermost borrowed block back. Borrowing is strictly innermost last.
        void ReturnState(size_t bytes)
        {
            const size_t given = AlignState(bytes);
            AZ_Assert(m_brainUsed >= given, "A nested backend cannot give back more than it borrowed");
            m_brainUsed = static_cast<AZ::u16>(m_brainUsed - AZStd::min(given, static_cast<size_t>(m_brainUsed)));
        }

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
