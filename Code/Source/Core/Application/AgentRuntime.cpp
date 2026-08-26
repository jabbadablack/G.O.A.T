#include <Core/Application/AgentRuntime.h>

namespace GOAT
{
    namespace
    {
        //! Most intents one tick will satisfy before deferring the rest.
        //! Bounds the work a tree of instantly completing leaves can do in a single frame.
        constexpr int MaxIntentsPerTick = 8;
    } // namespace

    AgentRuntime::AgentRuntime(
        IBlackboardSystem& blackboard,
        const ActionStateRegistry& actions,
        const BackendRegistry& backends,
        IBackend& directBackend,
        LuaDispatch& dispatch,
        AgentScriptContext& scriptContext,
        INodeScripting& scripting)
        : m_blackboard(blackboard)
        , m_actions(actions)
        , m_backends(backends)
        , m_directBackend(directBackend)
        , m_dispatch(dispatch)
        , m_scriptContext(scriptContext)
        , m_scripting(scripting)
    {
    }

    PlanContext AgentRuntime::MakePlanContext(AgentRecord& agent) const
    {
        PlanContext context;
        context.m_agent = agent.m_id;
        context.m_entity = agent.m_entity;
        context.m_blackboard = &m_blackboard;
        context.m_scripting = &m_scripting;
        return context;
    }

    ActionContext AgentRuntime::MakeActionContext(AgentRecord& agent) const
    {
        ActionContext context;
        context.m_agent = agent.m_id;
        context.m_entity = agent.m_entity;
        context.m_blackboard = &m_blackboard;
        return context;
    }

    bool AgentRuntime::ApplyGuards(
        AgentRecord& agent, const PlanContext& planContext, WalkStep& outStep, bool& outHaveStep)
    {
        outHaveStep = false;

        if (!agent.m_observer.IsDirty())
        {
            return false;
        }
        agent.m_observer.Clear();

        const AbortDecision decision = m_guards.Evaluate(*agent.m_program, agent.m_cursor, planContext);
        if (decision.m_action == AbortAction::None)
        {
            return false;
        }

        // The running action is interrupted either way, so end it before the walk moves.
        ActionContext actionContext = MakeActionContext(agent);
        agent.m_machine.Abort(m_actions, actionContext);

        if (decision.m_action == AbortAction::Restart)
        {
            outStep = m_walker.Restart(*agent.m_program, agent.m_cursor, planContext, decision.m_node);
        }
        else
        {
            // The guard stopped holding, so its branch fails from where the guard sits.
            agent.m_cursor.SetActiveLeaf(decision.m_node);
            outStep = m_walker.Advance(*agent.m_program, agent.m_cursor, planContext, ActionResult::Failure);
        }

        outHaveStep = true;
        return true;
    }

    void AgentRuntime::TickServices(AgentRecord& agent, float deltaTime)
    {
        m_services.CollectDue(*agent.m_program, agent.m_cursor, agent.m_dueServices);
        if (agent.m_dueServices.empty())
        {
            return;
        }

        m_scriptContext.Bind(agent.m_id, agent.m_entity, &m_blackboard);
        for (const AZ::u32 service : agent.m_dueServices)
        {
            const DecisionService& declared = agent.m_program->m_services[service];
            if (!declared.m_behavior.IsEmpty())
            {
                m_dispatch.CallBehavior(declared.m_behavior, "tick", agent.m_id, m_scriptContext, deltaTime);
            }
        }
        m_scriptContext.Unbind();
    }

    bool AgentRuntime::StartPlan(AgentRecord& agent, const PlanContext& planContext, const Intent& intent)
    {
        // Every intent goes through a backend, so a plainly authored leaf and a delegated
        // goal reach the state machine by exactly the same route.
        IBackend* backend = intent.m_backend.IsEmpty() ? &m_directBackend : m_backends.Find(intent.m_backend);
        if (backend == nullptr)
        {
            AZ_Warning("GOAT", false, "No backend named '%s' is installed", intent.m_backend.GetCStr());
            return false;
        }

        ActionPlan plan;
        if (!backend->Plan(planContext, intent, plan) || plan.IsEmpty())
        {
            return false;
        }

        agent.m_intent = intent;
        agent.m_machine.SetPlan(plan);
        return true;
    }

    void AgentRuntime::Tick(AgentRecord& agent, float deltaTime)
    {
        if (agent.m_program == nullptr || agent.m_program->IsEmpty())
        {
            return;
        }

        agent.m_cursor.AdvanceClock(deltaTime);
        const PlanContext planContext = MakePlanContext(agent);

        WalkStep step;
        bool haveStep = false;
        ApplyGuards(agent, planContext, step, haveStep);

        TickServices(agent, deltaTime);

        if (!haveStep)
        {
            if (agent.m_machine.HasPlan())
            {
                ActionContext actionContext = MakeActionContext(agent);
                const ActionResult result = agent.m_machine.Step(m_actions, actionContext, deltaTime);
                if (result == ActionResult::Running)
                {
                    return;
                }
                step = m_walker.Advance(*agent.m_program, agent.m_cursor, planContext, result);
            }
            else
            {
                step = m_walker.Begin(*agent.m_program, agent.m_cursor, planContext);
            }
            haveStep = true;
        }

        // Satisfy intents until one takes time to run, so a tree of instant leaves makes
        // progress without spinning the frame.
        for (int attempt = 0; attempt < MaxIntentsPerTick; ++attempt)
        {
            if (step.m_outcome == WalkOutcome::Finished)
            {
                // The tree ran out of work, so it starts again from the root.
                step = m_walker.Begin(*agent.m_program, agent.m_cursor, planContext);
                if (step.m_outcome == WalkOutcome::Finished)
                {
                    return;
                }
            }

            if (StartPlan(agent, planContext, step.m_intent))
            {
                return;
            }

            // No backend could satisfy that leaf, so it fails and the walk carries on.
            step = m_walker.Advance(*agent.m_program, agent.m_cursor, planContext, ActionResult::Failure);
        }

        AZ_Warning(
            "GOAT", false, "Agent tree '%s' produced %d intents in one tick without running anything",
            agent.m_program->m_name.GetCStr(), MaxIntentsPerTick);
    }
} // namespace GOAT
