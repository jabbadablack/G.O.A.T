#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/PlanStore.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/RTTI/TypeInfo.h>

namespace GOAT
{
    //! A sequence of actions produced by a backend; the input to the FSM.
    //!
    //! A view into a PlanStore rather than a buffer of its own. That is what lets a plan be any
    //! length: an authored plan's steps are baked once and shared by every agent running it, so a
    //! five hundred step plan costs an agent the same sixteen bytes a one step plan does, and
    //! reaching a plan boundary copies nothing.
    //!
    //! The span stays valid while the store that issued it does. A borrowed span must be given
    //! back to that store when the plan ends or is aborted; a baked one never is.
    struct ActionPlan final
    {
        AZ_TYPE_INFO(ActionPlan, ActionPlanTypeId);

        //! True when the plan has no steps to run.
        bool IsEmpty() const { return m_span.IsEmpty(); }

        //! How many steps the plan holds.
        size_t Size() const { return m_span.m_count; }

        //! The step at an index, or nullptr when the index is past the end.
        const ActionRequest* GetStep(size_t index) const
        {
            return index < m_span.m_count ? m_span.m_steps + index : nullptr;
        }

        //! True when these steps were borrowed and are owed back to the store.
        bool IsBorrowed() const { return m_span.m_block != InvalidPlanBlock; }

        //! Where the steps live.
        PlanStore::Span m_span;
    };
} // namespace GOAT
