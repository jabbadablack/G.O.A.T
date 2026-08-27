#include <Backends/Htn/HtnBackend.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    HtnBackend::HtnBackend(IAgentSystem& host, IBlackboardSystem& blackboard)
        : m_host(host)
        , m_blackboard(blackboard)
    {
    }

    AZ::Name HtnBackend::GetBackendName()
    {
        return AZ_NAME_LITERAL("htn");
    }

    AZ::Name HtnBackend::GetName() const
    {
        return GetBackendName();
    }

    AZStd::vector<AZ::Name> HtnBackend::GetNodeTypes() const
    {
        return { AZ_NAME_LITERAL("domain"), AZ_NAME_LITERAL("task"), AZ_NAME_LITERAL("method"),
                 AZ_NAME_LITERAL("primitive"), AZ_NAME_LITERAL("subtask"), AZ_NAME_LITERAL("effect") };
    }

    size_t HtnBackend::GetStateSize() const
    {
        return 0;
    }

    CompileOutcome HtnBackend::Compile(const AZ::Name& name, const AuthoredNode& root)
    {
        const HtnCompiler compiler(m_host, m_blackboard);
        auto compiled = compiler.Compile(name, root);
        if (!compiled.IsSuccess())
        {
            return AZ::Failure(compiled.TakeError());
        }

        auto domain = AZStd::shared_ptr<HtnDomain>(aznew HtnDomain(AZStd::move(compiled.GetValue())));
        domain->m_backend = this;

        AZLOG_INFO("GOAT: domain '%s' compiled to %zu tasks, %zu methods and %zu variables",
            name.GetCStr(), domain->m_tasks.size(), domain->m_methods.size(), domain->m_touchedKeys.size());
        return AZ::Success(AZStd::shared_ptr<const AgentProgram>(AZStd::move(domain)));
    }

    Decision HtnBackend::Decide(const PlanContext& context, const AgentProgram& program, BrainState,
        ActionResult, float, ActionPlan& outPlan)
    {
        Decision decision;

        const HtnDomain& domain = static_cast<const HtnDomain&>(program);
        AZ_Assert(context.m_blackboard != nullptr, "Planning always runs with a blackboard");
        AZ_Assert(context.m_planStore != nullptr, "Producing a plan needs somewhere to put its steps");
        if (context.m_blackboard == nullptr || context.m_planStore == nullptr)
        {
            return decision;
        }

        WorkingState state;
        state.Snapshot(domain, *context.m_blackboard, context.m_agent);

        HtnPlanBuffer steps;
        if (!m_planner.Plan(domain, domain.m_root, state, steps))
        {
            AZLOG(GoatPlan, "GOAT: domain '%s' found nothing for agent %u", domain.m_name.GetCStr(),
                context.m_agent.GetIndex());
            return decision;
        }

        outPlan.m_span = context.m_planStore->Acquire(steps.data(), aznumeric_cast<AZ::u32>(steps.size()));
        decision.m_planned = true;

        AZLOG(GoatPlan, "GOAT: domain '%s' planned %zu step(s) for agent %u", domain.m_name.GetCStr(),
            steps.size(), context.m_agent.GetIndex());
        return decision;
    }
} // namespace GOAT
