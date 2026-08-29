#pragma once

#include <DecisionCursor.h>
#include <GuardEvaluator.h>
#include <ServiceTracker.h>
#include <TreeWalker.h>
#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IDecisionBackend.h>

namespace GOAT
{
    //! Runs agents by walking a compiled behaviour tree.
    class BehaviorTreeBackend final
        : public IDecisionBackend
    {
    public:
        AZ_RTTI(BehaviorTreeBackend, "{2B7F49A1-63C8-4E5D-9A02-8F1D4C60E735}", IDecisionBackend);
        AZ_CLASS_ALLOCATOR(BehaviorTreeBackend, AZ::SystemAllocator);

        BehaviorTreeBackend(IAgentSystem& host, IBlackboardSystem& blackboard);

        //! Name a tree agent is registered under.
        static AZ::Name GetBackendName();

        AZ::Name GetName() const override;
        AZStd::vector<AZ::Name> GetNodeTypes() const override;
        size_t GetStateSize() const override;
        CompileOutcome Compile(const AZ::Name& name, const AuthoredNode& root) override;
        void Attach(const PlanContext& context, const AgentProgram& program, BrainState state) override;
        TickResult Advance(
            const PlanContext& context, const AgentProgram& program, BrainState state, float elapsed,
            size_t runningStep) override;
        Decision Decide(const PlanContext& context, const AgentProgram& program, BrainState state,
            ActionResult lastResult, float elapsed, ActionPlan& outPlan) override;
        void DescribePosition(const AgentProgram& program, BrainState state, size_t runningStep,
            AZStd::vector<ProgramNodeRef>& outPath) const override;

    private:
        //! The cursor an agent keeps inside its brain state.
        static DecisionCursor& Cursor(BrainState state);

        //! Runs the services whose subtree the agent is currently inside.
        void TickServices(const PlanContext& context, const DecisionProgram& program, DecisionCursor& cursor);

        //! Turns an intent into a plan: the leaf's own request, or whatever it delegated to.
        bool SatisfyIntent(const PlanContext& context, const DecisionProgram& program,
            const Intent& intent, ActionPlan& outPlan) const;

        IAgentSystem& m_host;
        IBlackboardSystem& m_blackboard;
        TreeWalker m_walker;
        GuardEvaluator m_guards;
        ServiceTracker m_services;
    };
} // namespace GOAT
