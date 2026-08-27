#pragma once

#include <Backends/Htn/HtnCompiler.h>
#include <Backends/Htn/HtnPlanner.h>

#include <GOAT/Interfaces/IDecisionBackend.h>

namespace GOAT
{
    //! Runs agents by decomposing a task network into a plan of verbs.
    class HtnBackend final
        : public IDecisionBackend
    {
    public:
        AZ_RTTI(HtnBackend, "{6D2A81F4-95C7-4E30-B1A8-3F07C64D2E59}", IDecisionBackend);
        AZ_CLASS_ALLOCATOR(HtnBackend, AZ::SystemAllocator);

        HtnBackend(const NodeTypeRegistry& nodeTypes, IBlackboardSystem& blackboard,
            const ActionStateRegistry& actions);

        //! Name an entity asks for to be run by a task network.
        static AZ::Name GetBackendName();

        AZ::Name GetName() const override;
        AZStd::vector<AZ::Name> GetNodeTypes() const override;
        size_t GetStateSize() const override;
        CompileOutcome Compile(const AZ::Name& name, const AuthoredNode& root) override;
        Decision Decide(const PlanContext& context, const AgentProgram& program, BrainState state,
            ActionResult lastResult, float elapsed, ActionPlan& outPlan) override;

    private:
        const NodeTypeRegistry& m_nodeTypes;
        IBlackboardSystem& m_blackboard;
        const ActionStateRegistry& m_actions;
        HtnPlanner m_planner;
    };
} // namespace GOAT
