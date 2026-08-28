#pragma once

#include <Core/Application/AgentRecord.h>

#include <GOAT/Domain/AgentProgram.h>
#include <GOAT/Domain/AgentStateMachine.h>
#include <GOAT/Interfaces/IDecisionBackend.h>

namespace GOAT
{
    class ActionStateRegistry;
    struct ActionContext;

    //! One paradigm running inside another, held in the borrowed part of an agent's brain block.
    //!
    //! It carries its own state machine because a plan is run one step at a time and the host is
    //! already running one of its own. Nesting happens in deciding, not in running: only the
    //! innermost frame produces a plan, so the agent still only ever has one action in flight.
    struct NestedFrame final
    {
        //! The foreign program this frame runs. The core owns it, as it owns every program.
        const AgentProgram* m_program = nullptr;
        //! The plan it is running and how far into it it is.
        AgentStateMachine m_machine;
        //! Where its backend keeps its state, which is the block right after this frame.
        BrainState m_state;
        //! How its last plan ended, which is what its backend is asked to decide from.
        ActionResult m_last = ActionResult::Success;
        //! True once it has produced a plan, so a program that never ran can be told apart from
        //! one that ran and finished.
        bool m_ran = false;
    };

    //! What one nesting level costs an agent, on top of what the nested program itself needs.
    inline constexpr size_t NestedFrameBytes()
    {
        return AlignState(sizeof(NestedFrame));
    }

    //! Starts a program running inside an agent, in the next free part of its brain block.
    //! Null when there is no room, which means the total folded at compile time was wrong.
    NestedFrame* EnterNested(AgentRecord& record, const AgentProgram& program, const PlanContext& context);

    //! Ends a nested run, giving back its plan and its part of the block, innermost first.
    void LeaveNested(
        AgentRecord& record, NestedFrame& frame, const ActionStateRegistry& actions, ActionContext& context,
        const PlanContext& planContext);
} // namespace GOAT
