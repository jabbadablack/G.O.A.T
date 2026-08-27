#pragma once

#include <Backends/Htn/HtnCompiler.h>
#include <Backends/Htn/HtnPlanner.h>

#include <GOAT/Interfaces/IDecisionBackend.h>

namespace GOAT
{
    //! The plan an agent is running, as the tasks it came from, so what is left of it can be
    //! re-checked against the world without planning again.
    struct HtnPlanRecord final
    {
        AZ::u16 m_count = 0;
        AZ::u16 m_tasks[MaxPlanTasks]{};
    };

    //! Runs agents by decomposing a task network into a plan of verbs.
    class HtnBackend final
        : public IDecisionBackend
    {
    public:
        AZ_RTTI(HtnBackend, "{6D2A81F4-95C7-4E30-B1A8-3F07C64D2E59}", IDecisionBackend);
        AZ_CLASS_ALLOCATOR(HtnBackend, AZ::SystemAllocator);

        HtnBackend(IAgentSystem& host, IBlackboardSystem& blackboard);

        //! Name an entity asks for to be run by a task network.
        static AZ::Name GetBackendName();

        AZ::Name GetName() const override;
        AZStd::vector<AZ::Name> GetNodeTypes() const override;
        size_t GetStateSize() const override;
        CompileOutcome Compile(const AZ::Name& name, const AuthoredNode& root) override;
        void Attach(const PlanContext& context, const AgentProgram& program, BrainState state) override;
        TickResult Advance(const PlanContext& context, const AgentProgram& program, BrainState state,
            float elapsed, size_t runningStep) override;
        Decision Decide(const PlanContext& context, const AgentProgram& program, BrainState state,
            ActionResult lastResult, float elapsed, ActionPlan& outPlan) override;

    private:
        //! The plan record an agent keeps inside its brain state.
        static HtnPlanRecord& Record(BrainState state);

        IAgentSystem& m_host;
        IBlackboardSystem& m_blackboard;
        HtnPlanner m_planner;
    };
} // namespace GOAT
