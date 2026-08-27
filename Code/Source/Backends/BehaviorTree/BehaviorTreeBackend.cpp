#include <Backends/BehaviorTree/BehaviorTreeBackend.h>

#include <Backends/BehaviorTree/TreeCompiler.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    namespace
    {
        //! Most intents one decision will satisfy before giving up on making progress.
        constexpr int MaxIntentsPerDecision = 8;
    } // namespace

    BehaviorTreeBackend::BehaviorTreeBackend(
        const NodeTypeRegistry& nodeTypes,
        IBlackboardSystem& blackboard,
        const TreeLibrary& trees,
        const ActionStateRegistry& actions,
        const BackendRegistry& backends,
        LuaDispatch& dispatch,
        AgentScriptContext& scriptContext)
        : m_nodeTypes(nodeTypes)
        , m_blackboard(blackboard)
        , m_trees(trees)
        , m_actions(actions)
        , m_backends(backends)
        , m_dispatch(dispatch)
        , m_scriptContext(scriptContext)
    {
    }

    AZ::Name BehaviorTreeBackend::GetBackendName()
    {
        return AZ_NAME_LITERAL("tree");
    }

    AZ::Name BehaviorTreeBackend::GetName() const
    {
        return GetBackendName();
    }

    AZStd::vector<AZ::Name> BehaviorTreeBackend::GetNodeTypes() const
    {
        AZStd::vector<AZ::Name> names;
        for (const NodeTypeDescriptor* descriptor : m_nodeTypes.GetAll())
        {
            names.push_back(descriptor->m_name);
        }
        return names;
    }

    size_t BehaviorTreeBackend::GetStateSize() const
    {
        return sizeof(DecisionCursor);
    }

    DecisionCursor& BehaviorTreeBackend::Cursor(BrainState state)
    {
        AZ_Assert(state.size() >= sizeof(DecisionCursor), "An agent's brain state must hold a cursor");
        return *reinterpret_cast<DecisionCursor*>(state.data());
    }

    CompileOutcome BehaviorTreeBackend::Compile(const AZ::Name& name, const AuthoredNode& root)
    {
        const TreeCompiler compiler(m_nodeTypes, m_blackboard, m_trees, m_actions);
        auto compiled = compiler.Compile(name, root);
        if (!compiled.IsSuccess())
        {
            return AZ::Failure(compiled.TakeError());
        }

        auto program = AZStd::shared_ptr<DecisionProgram>(aznew DecisionProgram(AZStd::move(compiled.GetValue())));
        program->m_backend = this;
        return AZ::Success(AZStd::shared_ptr<const AgentProgram>(AZStd::move(program)));
    }

    void BehaviorTreeBackend::Attach(
        [[maybe_unused]] const PlanContext& context, const AgentProgram& program, BrainState state)
    {
        AZ_Assert(state.size() >= sizeof(DecisionCursor), "An agent's brain state must hold a cursor");

        DecisionCursor* cursor = new (state.data()) DecisionCursor();
        cursor->Reset(static_cast<const DecisionProgram&>(program));
    }

    TickResult BehaviorTreeBackend::Advance(
        const PlanContext& context, const AgentProgram& program, BrainState state, float elapsed)
    {
        const DecisionProgram& tree = static_cast<const DecisionProgram&>(program);
        DecisionCursor& cursor = Cursor(state);
        cursor.AdvanceClock(elapsed);

        TickServices(context, tree, cursor);

        const AbortDecision decision = m_guards.Evaluate(tree, cursor, context);
        if (decision.m_action == AbortAction::None)
        {
            return TickResult::Continue;
        }

        // A failing guard fails its own branch from where it sits; a higher priority one that
        // started holding sends the walk back to the root, which is where it would be found.
        cursor.SetActiveLeaf(decision.m_action == AbortAction::Fail ? decision.m_node : InvalidNodeIndex);

        AZLOG(GoatAgent, "GOAT: agent %u aborted at node %u (%s)", context.m_agent.GetIndex(), decision.m_node,
            decision.m_action == AbortAction::Fail ? "guard closed" : "higher priority opened");
        return TickResult::Abandon;
    }

    void BehaviorTreeBackend::TickServices(
        const PlanContext& context, const DecisionProgram& program, DecisionCursor& cursor)
    {
        DueServices due;
        m_services.CollectDue(program, cursor, due);
        if (due.empty())
        {
            return;
        }

        m_scriptContext.Bind(context.m_agent, context.m_entity, context.m_blackboard);
        for (const AZ::u32 service : due)
        {
            AZ_Assert(service < program.m_services.size(), "A due service must address a compiled service");

            const DecisionService& declared = program.m_services[service];
            if (!declared.m_behavior.IsEmpty())
            {
                m_dispatch.CallBehavior(declared.m_behavior, "tick", context.m_agent, m_scriptContext,
                    declared.m_interval);
            }
        }
        m_scriptContext.Unbind();
    }

    bool BehaviorTreeBackend::SatisfyIntent(
        const PlanContext& context, const DecisionProgram& program, const Intent& intent, ActionPlan& outPlan) const
    {
        AZ_Assert(context.m_planStore != nullptr, "Producing a plan needs somewhere to put its steps");
        AZ_Assert(intent.m_node < program.m_nodes.size(), "An intent always comes from a node in the program");

        // An inline leaf is the whole plan already. Handing it to a backend to be copied back
        // out again was a round trip that decided nothing.
        if (intent.m_backend.IsEmpty())
        {
            const ActionRequest& request = program.m_nodes[intent.m_node].m_action;
            if (request.m_action == CoreActions::Invalid)
            {
                AZ_Error("GOAT", false, "Node %u names no registered verb", intent.m_node);
                return false;
            }

            outPlan.m_span = context.m_planStore->Acquire(&request, 1);
            return true;
        }

        IBackend* backend = m_backends.Find(intent.m_backend);
        if (backend == nullptr)
        {
            AZ_Warning("GOAT", false, "No backend named '%s' is installed", intent.m_backend.GetCStr());
            return false;
        }

        if (!backend->Plan(context, intent, outPlan) || outPlan.IsEmpty())
        {
            AZLOG(GoatAgent, "GOAT: backend '%s' refused node %u for agent %u", backend->GetName().GetCStr(),
                intent.m_node, context.m_agent.GetIndex());
            return false;
        }

        AZLOG(GoatAgent, "GOAT: agent %u node %u -> backend '%s' produced %zu step(s)",
            context.m_agent.GetIndex(), intent.m_node, backend->GetName().GetCStr(), outPlan.Size());
        return true;
    }

    Decision BehaviorTreeBackend::Decide(const PlanContext& context, const AgentProgram& program, BrainState state,
        ActionResult lastResult, float elapsed, ActionPlan& outPlan)
    {
        const DecisionProgram& tree = static_cast<const DecisionProgram&>(program);
        DecisionCursor& cursor = Cursor(state);
        cursor.AdvanceClock(elapsed);

        Decision decision;
        if (tree.IsEmpty())
        {
            AZ_Error("GOAT", false, "Agent %u is running a program with no nodes", context.m_agent.GetIndex());
            return decision;
        }

        WalkStep step = m_walker.Advance(tree, cursor, context, lastResult);

        // Tracks whether the step in hand came from a walk that started at the root, because a
        // walk that started there and found nothing cannot find anything by starting again.
        bool walkedFromRoot = cursor.GetActiveLeaf() == InvalidNodeIndex;

        for (int attempt = 0; attempt < MaxIntentsPerDecision; ++attempt)
        {
            if (step.m_outcome == WalkOutcome::Finished)
            {
                if (walkedFromRoot)
                {
                    decision.m_wakeIn = AZStd::max(step.m_wakeAt - cursor.GetNow(), 0.0f);
                    return decision;
                }

                step = m_walker.Begin(tree, cursor, context);
                walkedFromRoot = true;
                if (step.m_outcome == WalkOutcome::Finished)
                {
                    decision.m_wakeIn = AZStd::max(step.m_wakeAt - cursor.GetNow(), 0.0f);
                    return decision;
                }
            }

            AZ_Assert(step.m_outcome == WalkOutcome::Intent, "A walk that is not finished must carry an intent");

            if (SatisfyIntent(context, tree, step.m_intent, outPlan))
            {
                decision.m_planned = true;
                return decision;
            }

            step = m_walker.Advance(tree, cursor, context, ActionResult::Failure);
        }

        AZ_Warning("GOAT", false, "Agent program '%s' produced %d intents at once without running anything",
            tree.m_name.GetCStr(), MaxIntentsPerDecision);
        return decision;
    }
} // namespace GOAT
