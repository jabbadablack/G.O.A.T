#pragma once

#include <Core/Application/ActionStateRegistry.h>

#include <GOAT/Domain/ActionPlan.h>
#include <GOAT/Domain/PlanStore.h>
#include <GOAT/GOATTypeIds.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! Assembles a plan from the steps a Lua backend returns.
    //!
    //! Lua pushes into this, so a backend written in Lua produces exactly the plan a C++ backend
    //! would and reaches the state machine by the same route. Steps land in a scratch buffer that
    //! is reused for the lifetime of the gem, so building a plan allocates nothing once the buffer
    //! has grown to the largest plan a project uses.
    //!
    //! An authored plan skips all of this: its steps were baked when the vocabulary loaded, and
    //! Lua names the option it chose rather than pushing the steps again.
    class LuaPlanBuilder final
    {
    public:
        AZ_TYPE_INFO(LuaPlanBuilder, LuaPlanBuilderTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Points the builder at the registries it needs to resolve names, and the store it
        //! bakes into and borrows from.
        void Configure(const ActionStateRegistry* actions, const IBlackboardSystem* blackboard, PlanStore* store);

        //! Starts a plan, discarding anything previously built.
        void BeginPlan();

        //! Appends a step running a named verb. Unknown verbs make the plan fail.
        void AddStep(AZStd::string verb);

        //! Sets a property on the most recently added step.
        void SetTag(AZStd::string tag);
        void SetDuration(double seconds);
        void SetTolerance(double tolerance);
        void SetTargetKey(AZStd::string blackboardName);
        void SetTargetPosition(const AZ::Vector3& position);
        void SetTargetEntity(AZ::EntityId entity);

        //! Records which authored plan and option produced this, purely so the trace can say so.
        //! An imperative backend never calls it, which is how tracing tells the two apart.
        void SetSource(AZStd::string plan, double option);

        //! Hands back a plan whose steps were baked when the vocabulary loaded, rather than
        //! rebuilding them. This is the fast path an authored plan takes every time it runs.
        bool ChooseBaked(AZStd::string plan, double option);

        //! Finishes the plan, borrowing room for the steps. False when a step named something
        //! that is not registered, or the plan ran past the runaway guard.
        bool EndPlan();

        //! Bakes the steps built so far and remembers them as one option of an authored plan.
        //! Called while the vocabulary loads, never while an agent is running.
        bool BakeOption(AZStd::string plan, double option);

        //! Drops every baked option. The store's own steps go with it.
        void ClearBaked();

        //! The assembled plan, valid once EndPlan or ChooseBaked returned true.
        const ActionPlan& GetPlan() const { return m_plan; }

        //! Which authored plan and option produced the current plan, for tracing.
        const AZStd::string& GetSourcePlan() const { return m_sourcePlan; }
        int GetSourceOption() const { return m_sourceOption; }

        //! How many authored options are baked, for console output.
        size_t GetBakedOptionCount() const { return m_bakedOptions.size(); }

    private:
        //! One authored option's baked steps, found again by plan name and index.
        struct BakedOption final
        {
            AZ::Name m_plan;
            int m_option = 0;
            PlanStore::Span m_span;
        };

        const ActionStateRegistry* m_actions = nullptr;
        const IBlackboardSystem* m_blackboard = nullptr;
        PlanStore* m_store = nullptr;

        //! Steps under construction. Reused rather than reallocated, which is what keeps a plan
        //! boundary allocation free once a project's largest plan has been seen once.
        AZStd::vector<ActionRequest> m_scratch;
        AZStd::vector<BakedOption> m_bakedOptions;

        ActionPlan m_plan;
        AZStd::string m_sourcePlan;
        int m_sourceOption = 0;
        bool m_failed = false;
    };
} // namespace GOAT
