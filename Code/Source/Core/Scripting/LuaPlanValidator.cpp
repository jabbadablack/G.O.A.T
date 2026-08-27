#include <Core/Scripting/LuaPlanValidator.h>

#include <GOAT/Domain/BlackboardTypes.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>

namespace GOAT
{
    void LuaPlanValidator::Configure(const ActionStateRegistry* actions, const IBlackboardSystem* blackboard)
    {
        m_actions = actions;
        m_blackboard = blackboard;
    }

    void LuaPlanValidator::Reset()
    {
        m_problems.clear();
        m_summaries.clear();
        m_option = 0;
        m_step = 0;
        m_fallback = 0;

        AZ_Assert(IsClean(), "Resetting must leave no problem from a previous pass");
    }

    void LuaPlanValidator::Report(const char* what, const AZStd::string& detail)
    {
        AZ_Assert(what != nullptr, "A problem always says what it is about");
        AZ_Assert(!m_summaries.empty(), "A problem is only reported while a plan is open");

        if (m_summaries.empty())
        {
            return;
        }

        const PlanSummary& plan = m_summaries.back();

        // The location is where the plan was written, not where it is being checked: validation
        // may run much later, or from the console, and only the declaration is worth pointing at.
        AZStd::string where = AZStd::string::format(
            "plan '%s' (%s)", plan.m_name.GetCStr(), plan.m_source.c_str());

        if (m_option > 0)
        {
            where += AZStd::string::format(" option %d", m_option);
        }
        if (m_step > 0)
        {
            where += AZStd::string::format(" step %d", m_step);
        }

        m_problems.push_back(AZStd::string::format("%s: %s %s", where.c_str(), what, detail.c_str()));
    }

    void LuaPlanValidator::BeginPlan(AZStd::string name, AZStd::string source)
    {
        PlanSummary summary;
        summary.m_name = AZ::Name(name);
        summary.m_source = AZStd::move(source);
        m_summaries.push_back(AZStd::move(summary));

        m_option = 0;
        m_step = 0;
        m_fallback = 0;
    }

    void LuaPlanValidator::BeginOption(AZStd::string guard, bool negated)
    {
        AZ_Assert(!m_summaries.empty(), "An option always belongs to a plan");
        if (m_summaries.empty())
        {
            return;
        }

        ++m_option;
        m_step = 0;

        OptionSummary option;
        option.m_guard = guard;
        option.m_negated = negated;
        m_summaries.back().m_options.push_back(AZStd::move(option));

        if (guard.empty())
        {
            // An unguarded option always runs, so anything after it is dead. Remembered rather
            // than reported here, because "not last" is only knowable once the plan ends.
            if (m_fallback == 0)
            {
                m_fallback = m_option;
            }
            return;
        }

        AZ_Assert(m_blackboard != nullptr, "Checking a guard needs the blackboard system");
        if (m_blackboard == nullptr)
        {
            return;
        }

        const BlackboardKey key = m_blackboard->FindKey(AZ::Name(guard));
        if (!key.IsValid())
        {
            Report("guard", AZStd::string::format("'%s' is not a declared blackboard variable", guard.c_str()));
            return;
        }

        // Checked here rather than left to a runtime assert, because a guard that reads the wrong
        // type is silently always false, which looks exactly like an option that never matches.
        if (key.GetType() != BlackboardType::Bool)
        {
            Report("guard", AZStd::string::format(
                "'%s' is declared as %s, but a guard reads a Bool", guard.c_str(), ToString(key.GetType())));
        }
    }

    void LuaPlanValidator::CheckStep(AZStd::string verb)
    {
        ++m_step;

        // Named before the registry lookup so the message explains the layering rather than
        // saying a tree word is merely unregistered.
        if (verb == "delegate")
        {
            Report("step", AZStd::string(
                "'delegate' is a tree word, not an action verb; a plan runs verbs and cannot "
                "re-enter the tree that asked for it"));
            return;
        }

        AZ_Assert(m_actions != nullptr, "Checking a verb needs the action registry");
        if (m_actions == nullptr)
        {
            return;
        }

        if (m_actions->FindId(AZ::Name(verb)) == CoreActions::Invalid)
        {
            Report("step", AZStd::string::format("verb '%s' is not registered", verb.c_str()));
        }
    }

    void LuaPlanValidator::CheckKey(AZStd::string blackboardName)
    {
        AZ_Assert(m_blackboard != nullptr, "Checking a key needs the blackboard system");
        if (m_blackboard == nullptr)
        {
            return;
        }

        if (!m_blackboard->FindKey(AZ::Name(blackboardName)).IsValid())
        {
            Report("step", AZStd::string::format(
                "'%s' is not a declared blackboard variable", blackboardName.c_str()));
        }
    }

    void LuaPlanValidator::Describe(AZStd::string line)
    {
        if (m_summaries.empty() || m_summaries.back().m_options.empty())
        {
            return;
        }

        m_summaries.back().m_options.back().m_lines.push_back(AZStd::move(line));
    }

    void LuaPlanValidator::EndOption()
    {
        AZ_Assert(!m_summaries.empty(), "An option always belongs to a plan");
        if (m_summaries.empty() || m_summaries.back().m_options.empty())
        {
            return;
        }

        if (m_summaries.back().m_options.back().m_lines.empty())
        {
            Report("option", AZStd::string("has no steps, so matching it would do nothing"));
        }

        m_step = 0;
    }

    void LuaPlanValidator::EndPlan()
    {
        AZ_Assert(!m_summaries.empty(), "A plan must have been begun before it ends");
        if (m_summaries.empty())
        {
            return;
        }

        const int optionCount = static_cast<int>(m_summaries.back().m_options.size());
        m_option = 0;
        m_step = 0;

        if (optionCount == 0)
        {
            Report("plan", AZStd::string("has no options"));
            return;
        }

        if (m_fallback != 0 && m_fallback != optionCount)
        {
            Report("plan", AZStd::string::format(
                "option %d has no guard, so options %d to %d can never run",
                m_fallback, m_fallback + 1, optionCount));
        }
    }

    const LuaPlanValidator::PlanSummary* LuaPlanValidator::FindSummary(const AZ::Name& plan) const
    {
        for (const PlanSummary& summary : m_summaries)
        {
            if (summary.m_name == plan)
            {
                return &summary;
            }
        }
        return nullptr;
    }

    void LuaPlanValidator::Reflect(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->Class<LuaPlanValidator>("GoatPlanValidator")
            ->Attribute(AZ::Script::Attributes::Category, "GOAT")
            ->Method("BeginPlan", &LuaPlanValidator::BeginPlan)
            ->Method("BeginOption", &LuaPlanValidator::BeginOption)
            ->Method("CheckStep", &LuaPlanValidator::CheckStep)
            ->Method("CheckKey", &LuaPlanValidator::CheckKey)
            ->Method("Describe", &LuaPlanValidator::Describe)
            ->Method("EndOption", &LuaPlanValidator::EndOption)
            ->Method("EndPlan", &LuaPlanValidator::EndPlan);
    }
} // namespace GOAT
