#pragma once

#include <Core/Application/ActionStateRegistry.h>

#include <GOAT/Domain/ActionPlan.h>
#include <GOAT/GOATTypeIds.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! Assembles an ActionPlan from the steps a Lua backend returns.
    //! Lua pushes into this, so a backend written in Lua produces exactly the same plan a
    //! C++ backend would and reaches the state machine by the same route.
    class LuaPlanBuilder final
    {
    public:
        AZ_TYPE_INFO(LuaPlanBuilder, LuaPlanBuilderTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Points the builder at the registries it needs to resolve names.
        void Configure(const ActionStateRegistry* actions, const IBlackboardSystem* blackboard);

        //! Starts a plan, discarding anything previously built.
        void BeginPlan();

        //! Appends a step running a named verb. Unknown verbs make the plan fail.
        void AddStep(AZStd::string verb);

        //! Sets a property on the most recently added step.
        void SetTag(AZStd::string tag);
        void SetDuration(double seconds);
        void SetTolerance(double tolerance);
        void SetTargetKey(AZStd::string blackboardName);

        //! Finishes the plan. Returns false when a step named something that is not registered.
        bool EndPlan();

        //! The assembled plan, valid once EndPlan returned true.
        const ActionPlan& GetPlan() const { return m_plan; }

    private:
        const ActionStateRegistry* m_actions = nullptr;
        const IBlackboardSystem* m_blackboard = nullptr;
        ActionPlan m_plan;
        bool m_failed = false;
    };
} // namespace GOAT
