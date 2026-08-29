#include <BehaviorTreeBackend.h>

#include <AzCore/std/algorithm.h>

#include <TreeCompiler.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    namespace
    {
        //! Most intents one decision will satisfy before giving up on making progress.
        constexpr int MaxIntentsPerDecision = 8;
    } // namespace

    BehaviorTreeBackend::BehaviorTreeBackend(IAgentSystem& host, IBlackboardSystem& blackboard)
        : m_host(host)
        , m_blackboard(blackboard)
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
        return { AZ_NAME_LITERAL("selector"), AZ_NAME_LITERAL("sequence"), AZ_NAME_LITERAL("parallel"),
                 AZ_NAME_LITERAL("invert"), AZ_NAME_LITERAL("force_success"), AZ_NAME_LITERAL("cooldown"),
                 AZ_NAME_LITERAL("loop"), AZ_NAME_LITERAL("conditional_loop"), AZ_NAME_LITERAL("time_limit"),
                 AZ_NAME_LITERAL("composite"), AZ_NAME_LITERAL("decorator"), AZ_NAME_LITERAL("service"),
                 AZ_NAME_LITERAL("subtree") };
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
        const TreeCompiler compiler(m_host, m_blackboard);
        auto compiled = compiler.Compile(name, root);
        if (!compiled.IsSuccess())
        {
            return AZ::Failure(compiled.TakeError());
        }

        auto program = AZStd::shared_ptr<DecisionProgram>(aznew DecisionProgram(AZStd::move(compiled.GetValue())));
        program->m_backend = this;
        return AZ::Success(AZStd::shared_ptr<AgentProgram>(AZStd::move(program)));
    }

    void BehaviorTreeBackend::DescribePosition(const AgentProgram& program, BrainState state,
        [[maybe_unused]] size_t runningStep, AZStd::vector<ProgramNodeRef>& outPath) const
    {
        outPath.clear();

        const auto* tree = azrtti_cast<const DecisionProgram*>(&program);
        if (tree == nullptr || state.size() < sizeof(DecisionCursor))
        {
            return;
        }

        const auto& cursor = *reinterpret_cast<const DecisionCursor*>(state.data());
        NodeIndex node = cursor.GetActiveLeaf();
        if (node == InvalidNodeIndex || node >= tree->m_authored.size())
        {
            // Idle, or a cursor left over from a program this agent is no longer running.
            return;
        }

        // Walk up from the leaf and turn the chain the right way round, because what a reader
        // wants to see is the path down to the node that is running.
        for (; node != InvalidNodeIndex; node = tree->m_nodes[node].m_parent)
        {
            AZ_Assert(node < tree->m_authored.size(), "A parent link always addresses a node of the same program");
            outPath.push_back(tree->m_authored[node]);
        }
        AZStd::reverse(outPath.begin(), outPath.end());
    }

    void BehaviorTreeBackend::Attach(
        [[maybe_unused]] const PlanContext& context, const AgentProgram& program, BrainState state)
    {
        AZ_Assert(state.size() >= sizeof(DecisionCursor), "An agent's brain state must hold a cursor");

        DecisionCursor* cursor = new (state.data()) DecisionCursor();
        cursor->Reset(static_cast<const DecisionProgram&>(program));
    }

    TickResult BehaviorTreeBackend::Advance(
        const PlanContext& context, const AgentProgram& program, BrainState state, float elapsed,
        [[maybe_unused]] size_t runningStep)
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

        for (const AZ::u32 service : due)
        {
            AZ_Assert(service < program.m_services.size(), "A due service must address a compiled service");

            const DecisionService& declared = program.m_services[service];
            if (!declared.m_behavior.IsEmpty())
            {
                m_host.CallBehavior(declared.m_behavior, "tick", context.m_agent, declared.m_interval);
            }
        }
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

        IBackend* backend = m_host.FindBackend(intent.m_backend);
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
                    decision.m_result = step.m_result;
                    return decision;
                }

                step = m_walker.Begin(tree, cursor, context);
                walkedFromRoot = true;
                if (step.m_outcome == WalkOutcome::Finished)
                {
                    decision.m_wakeIn = AZStd::max(step.m_wakeAt - cursor.GetNow(), 0.0f);
                    decision.m_result = step.m_result;
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
