#include <Core/Application/NestedRun.h>

#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    NestedFrame* EnterNested(AgentRecord& record, const AgentProgram& program, const PlanContext& context)
    {
        IDecisionBackend* backend = program.m_backend;
        AZ_Assert(backend != nullptr, "A program cannot be run nested without the backend that compiled it");
        if (backend == nullptr)
        {
            return nullptr;
        }

        const size_t stateBytes = backend->GetStateSize();
        const BrainState block = record.BorrowState(NestedFrameBytes() + AlignState(stateBytes));
        if (block.empty())
        {
            AZ_Error("GOAT", false,
                "Agent %u has no room to run '%s' nested; the size folded when it compiled is wrong",
                record.m_id.GetIndex(), program.m_name.GetCStr());
            return nullptr;
        }

        NestedFrame* frame = new (block.data()) NestedFrame();
        frame->m_program = &program;
        frame->m_state = BrainState(block.data() + NestedFrameBytes(), stateBytes);

        backend->Attach(context, program, frame->m_state);
        return frame;
    }

    void LeaveNested(
        AgentRecord& record, NestedFrame& frame, const ActionStateRegistry& actions, ActionContext& context,
        const PlanContext& planContext)
    {
        // The nested plan goes back before its backend is told the run is over, so nothing can
        // still be running through a backend that has released what it held.
        frame.m_machine.Abort(actions, context);

        const AgentProgram* program = frame.m_program;
        AZ_Assert(program != nullptr, "A frame being left must name the program it ran");
        if (program != nullptr && program->m_backend != nullptr)
        {
            program->m_backend->Release(planContext, frame.m_state);
        }

        const size_t stateBytes = program != nullptr && program->m_backend != nullptr
            ? program->m_backend->GetStateSize()
            : 0;

        frame.~NestedFrame();
        record.ReturnState(NestedFrameBytes() + AlignState(stateBytes));
    }
} // namespace GOAT
