#include <Core/Application/AgentRuntime.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/limits.h>

namespace GOAT
{
    namespace
    {
        //! Seconds to leave an agent alone for, from what its running action asked for.
        float WakeDelay(const WakeCondition& wake)
        {
            switch (wake.m_when)
            {
            case WakeWhen::AtTime:
                return AZStd::max(wake.m_in, 0.0f);
            case WakeWhen::OnSignal:
                return AZStd::numeric_limits<float>::max();
            default:
                return 0.0f;
            }
        }
    } // namespace

    AgentRuntime::AgentRuntime(
        IBlackboardSystem& blackboard,
        const ActionStateRegistry& actions,
        const BackendRegistry& backends,
        INodeScripting& scripting,
        PlanStore& planStore)
        : m_blackboard(blackboard)
        , m_actions(actions)
        , m_backends(backends)
        , m_scripting(scripting)
        , m_planStore(planStore)
    {
    }

    void AgentRuntime::ReleaseAgent(AgentRecord& agent)
    {
        const PlanContext context = MakePlanContext(agent);
        m_backends.ForEach(
            [&context](IBackend& backend)
            {
                backend.Release(context);
            });

        if (IDecisionBackend* backend = agent.GetBackend())
        {
            backend->Release(context, agent.GetState());
        }
    }

    void AgentRuntime::AbortAgent(AgentRecord& agent)
    {
        ActionContext context = MakeActionContext(agent);
        agent.m_machine.Abort(m_actions, context);

        // Nothing it was waiting for says anything about what it does next.
        agent.m_wakeIn = 0.0f;

        AZ_Assert(!agent.m_machine.HasPlan(), "Aborting must leave the agent with no plan");
    }

    ActionContext AgentRuntime::MakeActionContext(AgentRecord& agent) const
    {
        ActionContext context;
        context.m_agent = agent.m_id;
        context.m_entity = agent.m_entity;
        context.m_blackboard = &m_blackboard;
        return context;
    }

    PlanContext AgentRuntime::MakePlanContext(AgentRecord& agent) const
    {
        PlanContext context;
        context.m_agent = agent.m_id;
        context.m_entity = agent.m_entity;
        context.m_blackboard = &m_blackboard;
        context.m_scripting = &m_scripting;
        context.m_planStore = &m_planStore;
        return context;
    }

    void AgentRuntime::Tick(AgentRecord& agent, float deltaTime)
    {
        // Applied here, at the very top, because ctx:SetTree is reachable from a behaviour
        // running inside the Step below, and everything past this point holds references into
        // the program a switch would replace.
        if (agent.m_pendingSwitch != TreeSwitchKind::None && m_applySwitch)
        {
            m_applySwitch(agent);
        }

        IDecisionBackend* backend = agent.GetBackend();
        AZ_Assert(agent.m_program != nullptr, "A registered agent always holds a compiled program");
        if (agent.m_program == nullptr || backend == nullptr)
        {
            AZ_Error("GOAT", false, "Agent %u is registered without a backend to run it", agent.m_id.GetIndex());
            return;
        }

        AZ_Assert(deltaTime >= 0.0f, "An agent cannot be ticked backwards in time");
        agent.m_elapsed += deltaTime;

        const bool dirty = agent.m_observer.IsDirty();
        const bool wantsTick = agent.m_program->m_wantsTick;

        // Asleep: nothing it watches changed and whatever it was waiting for has not come due.
        // A running action counts, so an agent waiting out a timer costs one subtraction.
        if (!dirty && !wantsTick)
        {
            agent.m_wakeIn -= deltaTime;
            if (agent.m_wakeIn > 0.0f)
            {
                return;
            }
        }

        const PlanContext planContext = MakePlanContext(agent);

        // The agent's clock and its backend's are different clocks over the same wall time, so
        // both are given the whole span, but the backend is given it only once.
        const float elapsed = agent.m_elapsed;
        float backendElapsed = elapsed;
        agent.m_elapsed = 0.0f;

        bool abandoned = false;
        if (dirty || wantsTick)
        {
            agent.m_observer.Clear();
            const size_t runningStep =
                agent.m_machine.HasPlan() ? agent.m_machine.GetStepIndex() : NoRunningStep;
            if (backend->Advance(planContext, *agent.m_program, agent.GetState(), backendElapsed, runningStep) ==
                TickResult::Abandon)
            {
                AbortAgent(agent);
                abandoned = true;
            }
            backendElapsed = 0.0f;
        }

        ActionResult lastResult = abandoned ? ActionResult::Failure : ActionResult::Success;
        if (agent.m_machine.HasPlan())
        {
            ActionContext actionContext = MakeActionContext(agent);
            WakeCondition wake;
            lastResult = agent.m_machine.Step(m_actions, actionContext, elapsed, wake);
            if (lastResult == ActionResult::Running)
            {
                agent.m_wakeIn = WakeDelay(wake);
                return;
            }
        }

        ActionPlan plan;
        const Decision decision =
            backend->Decide(planContext, *agent.m_program, agent.GetState(), lastResult, elapsed, plan);

        if (!decision.m_planned || plan.IsEmpty())
        {
            agent.m_wakeIn = decision.m_wakeIn;
            return;
        }

        agent.m_machine.SetPlan(m_planStore, plan);
        AZ_Assert(agent.m_machine.HasPlan(), "Starting a plan must leave the state machine holding one");
    }
} // namespace GOAT
