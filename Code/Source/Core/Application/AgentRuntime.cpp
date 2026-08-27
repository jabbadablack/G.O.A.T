#include <Core/Application/AgentRuntime.h>

#include <AzCore/Console/IConsole.h>
#include <AzCore/Console/ILogger.h>

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
        INodeScripting& scripting,
        PlanStore& planStore)
        : m_blackboard(blackboard)
        , m_actions(actions)
        , m_backends(backends)
        , m_directBackend(directBackend)
        , m_dispatch(dispatch)
        , m_scriptContext(scriptContext)
        , m_scripting(scripting)
        , m_planStore(planStore)
    {
    }

    void AgentRuntime::ReleaseAgent(AgentRecord& agent)
    {
        m_backends.ReleaseAgent(MakePlanContext(agent));
    }

    void AgentRuntime::AbortAgent(AgentRecord& agent)
    {
        AZ_Assert(!agent.m_id.IsNull(), "Only a registered agent can be aborted");

        ActionContext actionContext = MakeActionContext(agent);
        agent.m_machine.Abort(m_actions, actionContext);

        // Aborting drops the plan, so the tree has to be walked again rather than left waiting
        // on whatever the last walk was blocked by.
        agent.m_wakeAt = 0.0f;

        AZ_Assert(!agent.m_machine.HasPlan(), "Aborting must leave the agent with no plan to continue");
    }

    PlanContext AgentRuntime::MakePlanContext(AgentRecord& agent) const
    {
        AZ_Assert(!agent.m_id.IsNull(), "A plan context is only made for a registered agent");

        PlanContext context;
        context.m_agent = agent.m_id;
        context.m_entity = agent.m_entity;
        context.m_blackboard = &m_blackboard;
        context.m_scripting = &m_scripting;
        context.m_planStore = &m_planStore;

        AZ_Assert(context.m_blackboard != nullptr, "Every plan context reaches the blackboard");
        return context;
    }

    ActionContext AgentRuntime::MakeActionContext(AgentRecord& agent) const
    {
        AZ_Assert(!agent.m_id.IsNull(), "An action context is only made for a registered agent");

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
        AZ_Assert(agent.m_program != nullptr, "Guards are only applied to an agent that has a program");

        if (!agent.m_observer.IsDirty())
        {
            return false;
        }
        agent.m_observer.Clear();
        AZ_Assert(!agent.m_observer.IsDirty(), "Clearing the observer must mark the agent clean");

        const AbortDecision decision = m_guards.Evaluate(*agent.m_program, agent.m_cursor, planContext);
        if (decision.m_action == AbortAction::None)
        {
            return false;
        }

        // The running action is interrupted either way, so end it before the walk moves.
        ActionContext actionContext = MakeActionContext(agent);
        agent.m_machine.Abort(m_actions, actionContext);

        // Aborting drops the plan, so the tree has to be walked again rather than left waiting
        // on whatever the last walk was blocked by.
        agent.m_wakeAt = 0.0f;

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

        AZLOG(GoatAgent, "GOAT: agent %u aborted at node %u (%s)", agent.m_id.GetIndex(), decision.m_node,
            decision.m_action == AbortAction::Restart ? "restart" : "fail");

        outHaveStep = true;
        return true;
    }

    void AgentRuntime::TickServices(AgentRecord& agent, float deltaTime)
    {
        AZ_Assert(agent.m_program != nullptr, "Services are only ticked for an agent that has a program");
        AZ_Assert(deltaTime >= 0.0f, "Services cannot be ticked backwards in time");

        m_services.CollectDue(*agent.m_program, agent.m_cursor, agent.m_dueServices);
        if (agent.m_dueServices.empty())
        {
            return;
        }

        m_scriptContext.Bind(agent.m_id, agent.m_entity, &m_blackboard);
        for (const AZ::u32 service : agent.m_dueServices)
        {
            AZ_Assert(service < agent.m_program->m_services.size(),
                "A due service index must address a compiled service");

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
            AZLOG(GoatAgent, "GOAT: backend '%s' refused node %u for agent %u",
                backend->GetName().GetCStr(), intent.m_node, agent.m_id.GetIndex());
            return false;
        }

        // Per agent tracing is a tag channel rather than a cvar of ours, so it is toggled the
        // way every other engine channel is: LoggerSystemComponent.EnableLog GoatAgent.
        AZLOG(GoatAgent, "GOAT: agent %u node %u -> backend '%s' produced %zu step(s)",
            agent.m_id.GetIndex(), intent.m_node, backend->GetName().GetCStr(), plan.Size());

        agent.m_machine.SetPlan(m_planStore, plan);

        AZ_Assert(agent.m_machine.HasPlan(), "Starting a plan must leave the state machine holding one");
        return true;
    }

    void AgentRuntime::Tick(AgentRecord& agent, float deltaTime)
    {
        // Applied here, at the very top, because ctx:SetTree is reachable from a behaviour
        // running inside the Step below, and everything past this point holds references into
        // the program and cursor a switch would replace.
        if (agent.m_pendingSwitch != TreeSwitchKind::None && m_applySwitch)
        {
            m_applySwitch(agent);
        }

        AZ_Assert(agent.m_program != nullptr, "A registered agent always holds a compiled program");
        if (agent.m_program == nullptr || agent.m_program->IsEmpty())
        {
            AZ_Error("GOAT", false, "Agent %u is registered without a runnable program", agent.m_id.GetIndex());
            return;
        }

        AZ_Assert(deltaTime >= 0.0f, "An agent cannot be ticked backwards in time");
        agent.m_cursor.AdvanceClock(deltaTime);

        // Dormant: the last walk of this tree found no work, and neither of the two things that
        // could change that has happened. A predicate reads the blackboard and a cooldown reads
        // the clock, so if no observed slot has changed and no cooldown has come due, walking
        // again would evaluate the same conditions and reach the same answer.
        if (!agent.m_machine.HasPlan() && !agent.m_observer.IsDirty() &&
            !agent.m_program->m_pollEveryTick && agent.m_cursor.GetNow() < agent.m_wakeAt)
        {
            return;
        }

        const PlanContext planContext = MakePlanContext(agent);

        WalkStep step;
        bool haveStep = false;
        ApplyGuards(agent, planContext, step, haveStep);

        TickServices(agent, deltaTime);

        // Tracks whether the step in hand came from a walk that started at the root, because a
        // walk that started there and found nothing cannot find anything by starting again.
        bool walkedFromRoot = false;

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
                walkedFromRoot = true;
            }
            haveStep = true;
        }

        // Satisfy intents until one takes time to run, so a tree of instant leaves makes
        // progress without spinning the frame.
        for (int attempt = 0; attempt < MaxIntentsPerTick; ++attempt)
        {
            if (step.m_outcome == WalkOutcome::Finished)
            {
                // Nothing between two walks of the same tree in one tick can change what a
                // predicate reads, so a root walk that already found nothing is the answer
                // rather than something to ask again.
                if (walkedFromRoot)
                {
                    agent.m_wakeAt = step.m_wakeAt;
                    return;
                }

                // The tree ran out of work, so it starts again from the root.
                step = m_walker.Begin(*agent.m_program, agent.m_cursor, planContext);
                walkedFromRoot = true;
                if (step.m_outcome == WalkOutcome::Finished)
                {
                    agent.m_wakeAt = step.m_wakeAt;
                    return;
                }
            }

            AZ_Assert(step.m_outcome == WalkOutcome::Intent, "A walk that is not finished must carry an intent");

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
