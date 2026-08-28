#include <HtnBackend.h>

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
        return sizeof(HtnPlanRecord);
    }

    HtnPlanRecord& HtnBackend::Record(BrainState state)
    {
        AZ_Assert(state.size() >= sizeof(HtnPlanRecord), "An agent's brain state must hold its plan record");
        return *reinterpret_cast<HtnPlanRecord*>(state.data());
    }

    void HtnBackend::Attach(const PlanContext&, const AgentProgram&, BrainState state)
    {
        AZ_Assert(state.size() >= sizeof(HtnPlanRecord), "An agent's brain state must hold its plan record");
        new (state.data()) HtnPlanRecord();
    }

    TickResult HtnBackend::Advance(const PlanContext& context, const AgentProgram& program, BrainState state,
        float, size_t runningStep)
    {
        const HtnPlanRecord& record = Record(state);
        if (runningStep == NoRunningStep || runningStep >= record.m_count || context.m_blackboard == nullptr)
        {
            return TickResult::Continue;
        }

        const HtnDomain& domain = static_cast<const HtnDomain&>(program);
        WorkingState world;
        world.Snapshot(domain, *context.m_blackboard, context.m_agent);

        // The steps already run assumed their effects, so replay them. Without this a step is
        // checked against a world the ones before it never touched, and a plan that depends on
        // its own earlier work would abandon itself the moment anything else moved.
        for (AZ::u16 i = 0; i < runningStep; ++i)
        {
            HtnPlanner::ApplyEffects(domain, domain.m_tasks[record.m_tasks[i]], world);
        }

        // Only what is left, and only the primitives. The method conditions that chose this plan
        // are deliberately not re-checked: a step of this very plan may have written one.
        for (AZ::u16 i = static_cast<AZ::u16>(runningStep); i < record.m_count; ++i)
        {
            const HtnTask& task = domain.m_tasks[record.m_tasks[i]];
            if (!HtnPlanner::Allows(domain, task, world))
            {
                AZLOG(GoatPlan, "GOAT: domain '%s' dropped agent %u's plan at step %u",
                    domain.m_name.GetCStr(), context.m_agent.GetIndex(), static_cast<AZ::u32>(i));
                return TickResult::Abandon;
            }
            HtnPlanner::ApplyEffects(domain, task, world);
        }

        return TickResult::Continue;
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

    Decision HtnBackend::Decide(const PlanContext& context, const AgentProgram& program, BrainState state,
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

        WorkingState world;
        world.Snapshot(domain, *context.m_blackboard, context.m_agent);

        HtnPlanBuffer trail;
        if (!m_planner.Plan(domain, domain.m_root, world, trail))
        {
            AZLOG(GoatPlan, "GOAT: domain '%s' found nothing for agent %u", domain.m_name.GetCStr(),
                context.m_agent.GetIndex());
            return decision;
        }

        AZStd::fixed_vector<ActionRequest, MaxPlanTasks> steps;
        HtnPlanRecord& record = Record(state);
        record.m_count = 0;
        for (const AZ::u16 task : trail)
        {
            steps.push_back(domain.m_tasks[task].m_action);
            record.m_tasks[record.m_count++] = task;
        }

        outPlan.m_span = context.m_planStore->Acquire(steps.data(), aznumeric_cast<AZ::u32>(steps.size()));
        decision.m_planned = true;

        AZLOG(GoatPlan, "GOAT: domain '%s' planned %zu step(s) for agent %u", domain.m_name.GetCStr(),
            steps.size(), context.m_agent.GetIndex());
        return decision;
    }
} // namespace GOAT
