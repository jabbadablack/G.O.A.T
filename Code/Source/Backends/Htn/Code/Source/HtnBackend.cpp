#include <HtnBackend.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    namespace
    {
        //! Names the primitive steps a plan holds, in the order they will run.
        //! Only ever called from inside an AZLOG, so it costs nothing with the tag off.
        AZStd::string DescribeSteps(const HtnDomain& domain, const AZ::u16* tasks, AZ::u16 count)
        {
            AZStd::string described;
            for (AZ::u16 i = 0; i < count; ++i)
            {
                described += i == 0 ? "" : " -> ";
                described += domain.m_tasks[tasks[i]].m_name.GetCStr();
            }
            return described.empty() ? AZStd::string("<empty>") : described;
        }

        //! Names the method each compound task was carried out by.
        AZStd::string DescribeChoices(const HtnDomain& domain, const HtnChoiceTrail& choices)
        {
            AZStd::string described;
            for (const HtnChoice& choice : choices)
            {
                described += described.empty() ? "" : ", ";
                described += AZStd::string::format(
                    "%s#%u", domain.m_tasks[choice.m_task].m_name.GetCStr(), static_cast<AZ::u32>(choice.m_method));
            }
            return described.empty() ? AZStd::string("<none>") : described;
        }

        //! Names the first condition of a task that no longer holds.
        AZStd::string DescribeBrokenCondition(
            const HtnDomain& domain, const HtnTask& task, const WorkingState& world,
            const IBlackboardSystem& blackboard)
        {
            for (AZ::u16 i = 0; i < task.m_conditionCount; ++i)
            {
                const HtnCondition& condition = domain.m_conditions[task.m_firstCondition + i];
                if (world.Get(domain, condition.m_key) != condition.m_expected)
                {
                    return AZStd::string::format("%s is not %s", blackboard.GetKeyName(condition.m_key).GetCStr(),
                        condition.m_expected ? "true" : "false");
                }
            }
            return "its conditions";
        }
    } // namespace

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
                AZLOG(GoatHtn, "GOAT: agent %u domain '%s' dropped its plan at step %u of %u ('%s'): %s",
                    context.m_agent.GetIndex(), domain.m_name.GetCStr(), static_cast<AZ::u32>(i),
                    static_cast<AZ::u32>(record.m_count), task.m_name.GetCStr(),
                    DescribeBrokenCondition(domain, task, world, *context.m_blackboard).c_str());
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
        return AZ::Success(AZStd::shared_ptr<AgentProgram>(AZStd::move(domain)));
    }

    Decision HtnBackend::Decide(const PlanContext& context, const AgentProgram& program, BrainState state,
        ActionResult lastResult, float, ActionPlan& outPlan)
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
        HtnChoiceTrail choices;
        if (!m_planner.Plan(domain, domain.m_root, world, trail, &choices))
        {
            AZLOG(GoatHtn, "GOAT: agent %u domain '%s' found no plan from task '%s'",
                context.m_agent.GetIndex(), domain.m_name.GetCStr(),
                domain.m_root < domain.m_tasks.size() ? domain.m_tasks[domain.m_root].m_name.GetCStr() : "<none>");

            // A network with nothing left to decompose is done, not idle: it has no clock and no
            // guards of its own, so waiting could only ever produce this same answer again.
            decision.m_result = lastResult == ActionResult::Failure ? ActionResult::Failure : ActionResult::Success;
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

        AZLOG(GoatHtn, "GOAT: agent %u domain '%s' planned %zu step(s): %s [chose %s]",
            context.m_agent.GetIndex(), domain.m_name.GetCStr(), steps.size(),
            DescribeSteps(domain, record.m_tasks, record.m_count).c_str(),
            DescribeChoices(domain, choices).c_str());
        return decision;
    }
} // namespace GOAT
