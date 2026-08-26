#pragma once

#include <Core/Application/ActionStateRegistry.h>

#include <GOAT/GOATTypeIds.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! Checks every declared plan against the registries when the vocabulary loads.
    //!
    //! A mistyped verb or blackboard name is otherwise invisible until the moment an agent first
    //! delegates to that goal, which may be minutes into a level and far from the mistake. This
    //! turns that into a message at load naming the plan, the option and the step.
    //!
    //! Deliberately a separate object from LuaPlanBuilder rather than a dry run through it: the
    //! builder holds the plan an agent is about to receive, and validating through it would
    //! clobber that plan the moment validation ran from anywhere but load.
    class LuaPlanValidator final
    {
    public:
        AZ_TYPE_INFO(LuaPlanValidator, LuaPlanValidatorTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! One option of a plan, kept so the console can print it without asking Lua again.
        struct OptionSummary final
        {
            AZStd::string m_guard;
            bool m_negated = false;
            AZStd::vector<AZStd::string> m_lines;
        };

        //! One declared plan, and where it was written.
        struct PlanSummary final
        {
            AZ::Name m_name;
            AZStd::string m_source;
            AZStd::vector<OptionSummary> m_options;
        };

        //! Points the validator at the registries it checks against.
        void Configure(const ActionStateRegistry* actions, const IBlackboardSystem* blackboard);

        //! Discards everything from a previous pass.
        void Reset();

        //! True when the last pass found nothing wrong.
        bool IsClean() const { return m_problems.empty(); }

        //! What the last pass found, each already naming the plan, option and step.
        const AZStd::vector<AZStd::string>& GetProblems() const { return m_problems; }

        //! Every plan seen in the last pass, for console output.
        const AZStd::vector<PlanSummary>& GetSummaries() const { return m_summaries; }

        //! One plan by name, or nullptr when it was not declared.
        const PlanSummary* FindSummary(const AZ::Name& plan) const;

        //! Pushed from Lua, in declaration order.
        void BeginPlan(AZStd::string name, AZStd::string source);
        void BeginOption(AZStd::string guard, bool negated);
        void CheckStep(AZStd::string verb);
        void CheckKey(AZStd::string blackboardName);
        void Describe(AZStd::string line);
        void EndOption();
        void EndPlan();

    private:
        //! Records a problem, prefixed with the plan, option and step it belongs to.
        void Report(const char* what, const AZStd::string& detail);

        const ActionStateRegistry* m_actions = nullptr;
        const IBlackboardSystem* m_blackboard = nullptr;

        AZStd::vector<AZStd::string> m_problems;
        AZStd::vector<PlanSummary> m_summaries;

        //! Where the push is up to, so a problem can name it.
        int m_option = 0;
        int m_step = 0;
        //! Which option had no guard, so a fallback that is not last can be reported.
        int m_fallback = 0;
    };
} // namespace GOAT
