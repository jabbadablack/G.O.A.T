#include <Core/Actions/EmbedAction.h>

#include <Core/Application/ActionStateRegistry.h>
#include <Core/Application/AgentRegistry.h>
#include <Core/Application/AgentRuntime.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    EmbedAction::EmbedAction(
        AgentRegistry& agents, AgentRuntime& runtime, const ActionStateRegistry& actions,
        const ProgramTable& programs)
        : m_agents(agents)
        , m_runtime(runtime)
        , m_actions(actions)
        , m_programs(programs)
    {
    }

    AZ::Name EmbedAction::GetName() const
    {
        return AZ_NAME_LITERAL("embed");
    }

    NestedFrame* EmbedAction::Frame(const ActionContext& context)
    {
        NestedFrame* frame = nullptr;
        AZ_Assert(context.m_scratch != nullptr, "A verb always runs with scratch of its own");
        if (context.m_scratch != nullptr)
        {
            memcpy(&frame, context.m_scratch->data(), sizeof(frame));
        }
        return frame;
    }

    void EmbedAction::SetFrame(const ActionContext& context, NestedFrame* frame)
    {
        AZ_Assert(context.m_scratch != nullptr, "A verb always runs with scratch of its own");
        if (context.m_scratch != nullptr)
        {
            memcpy(context.m_scratch->data(), &frame, sizeof(frame));
        }
    }

    void EmbedAction::Begin(const ActionContext& context)
    {
        SetFrame(context, nullptr);

        AZ_Assert(context.m_request != nullptr, "A verb always runs from a request");
        const AZ::Name named = context.m_request != nullptr ? context.m_request->m_tag : AZ::Name{};

        const auto found = m_programs.find(named);
        if (found == m_programs.end() || found->second == nullptr)
        {
            AZ_Error("GOAT", false, "Agent %u cannot run '%s' inside what it is doing: no such program",
                context.m_agent.GetIndex(), named.GetCStr());
            return;
        }

        AgentRecord* record = m_agents.Find(context.m_agent);
        if (record == nullptr)
        {
            return;
        }

        NestedFrame* frame = EnterNested(*record, *found->second, m_runtime.MakePlanContext(*record));
        SetFrame(context, frame);

        AZLOG(GoatAgent, "GOAT: agent %u entered '%s' inside its plan", context.m_agent.GetIndex(),
            named.GetCStr());
    }

    ActionResult EmbedAction::Step(const ActionContext& context, float deltaTime)
    {
        NestedFrame* frame = Frame(context);
        AgentRecord* record = m_agents.Find(context.m_agent);
        if (frame == nullptr || record == nullptr || frame->m_program == nullptr)
        {
            return ActionResult::Failure;
        }

        IDecisionBackend* backend = frame->m_program->m_backend;
        AZ_Assert(backend != nullptr, "A program being run nested always knows what runs it");
        if (backend == nullptr)
        {
            return ActionResult::Failure;
        }

        const PlanContext planContext = m_runtime.MakePlanContext(*record);

        // Its own context, because the nested plan has its own step and its own scratch. Only
        // the agent and the blackboard are the same, and those are already right.
        ActionContext nested = context;

        // Re-check whatever could interrupt it. This is where the nested program's own guards
        // fire, and the host never hears about it: it drops its plan and decides again.
        const size_t running = frame->m_machine.HasPlan() ? frame->m_machine.GetStepIndex() : NoRunningStep;
        if (backend->Advance(planContext, *frame->m_program, frame->m_state, deltaTime, running) ==
            TickResult::Abandon)
        {
            frame->m_machine.Abort(m_actions, nested);
            frame->m_last = ActionResult::Failure;
        }

        if (frame->m_machine.HasPlan())
        {
            WakeCondition wake;
            const ActionResult result = frame->m_machine.Step(m_actions, nested, deltaTime, wake);
            if (result == ActionResult::Running)
            {
                // What the nested action is waiting for is what the host is waiting for, so the
                // agent can go dormant inside a nested run exactly as it would outside one.
                *context.m_wake = frame->m_program->m_wantsTick ? WakeCondition{} : wake;
                return ActionResult::Running;
            }
            frame->m_last = result;
        }

        ActionPlan plan;
        const Decision decision =
            backend->Decide(planContext, *frame->m_program, frame->m_state, frame->m_last, deltaTime, plan);

        if (decision.m_planned && !plan.IsEmpty())
        {
            frame->m_machine.SetPlan(*planContext.m_planStore, plan);
            frame->m_ran = true;
            *context.m_wake = WakeCondition{};
            return ActionResult::Running;
        }

        if (decision.m_result != ActionResult::Running)
        {
            // A program that never got as far as a plan did nothing, whatever it says about how
            // it ended, so the step it sits in failed.
            return frame->m_ran ? decision.m_result : ActionResult::Failure;
        }

        context.m_wake->m_when = WakeWhen::AtTime;
        context.m_wake->m_in = decision.m_wakeIn;
        if (frame->m_program->m_wantsTick)
        {
            *context.m_wake = WakeCondition{};
        }
        return ActionResult::Running;
    }

    void EmbedAction::End(const ActionContext& context)
    {
        NestedFrame* frame = Frame(context);
        AgentRecord* record = m_agents.Find(context.m_agent);
        if (frame == nullptr || record == nullptr)
        {
            return;
        }

        ActionContext nested = context;
        LeaveNested(*record, *frame, m_actions, nested, m_runtime.MakePlanContext(*record));
        SetFrame(context, nullptr);
    }
} // namespace GOAT
