#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/fixed_vector.h>

namespace GOAT
{
    //! Most action steps one backend may return for a single intent.
    inline constexpr size_t MaxPlanLength = 8;

    //! A short sequence of actions produced by a backend; the input to the FSM.
    struct ActionPlan final
    {
        AZ_TYPE_INFO(ActionPlan, ActionPlanTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! True when the plan has no steps left to run.
        bool IsEmpty() const { return m_steps.empty(); }

        //! Actions to run in order.
        AZStd::fixed_vector<ActionRequest, MaxPlanLength> m_steps;
    };
} // namespace GOAT
