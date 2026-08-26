#pragma once

#include <GOAT/Domain/ActionPlan.h>
#include <GOAT/Domain/ActionState.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/RTTI/TypeInfo.h>

#include <GOAT/Interfaces/IActionState.h>

namespace GOAT
{
    class ActionStateRegistry;
    struct ActionContext;

    //! The whole runtime state of one agent: the plan it is running and how far into it it is.
    //! Small and explicit so an agent can be inspected, dumped, or saved.
    class AgentStateMachine final
    {
    public:
        AZ_TYPE_INFO(AgentStateMachine, AgentStateMachineTypeId);

        //! Replaces the running plan and arms its first step. Does not end the previous action.
        //! Gives back whatever the previous plan borrowed, so a plan boundary leaks nothing.
        void SetPlan(PlanStore& store, const ActionPlan& plan);

        //! Gives back whatever the plan borrowed and leaves the machine with none.
        //! Called when an agent goes away, which is the one path Abort does not cover.
        void ReleasePlan();

        //! Advances the running action, moving to the next step when one finishes.
        //! Returns Running while the plan still has work, otherwise how the plan ended.
        ActionResult Step(const ActionStateRegistry& registry, ActionContext& context, float deltaTime);

        //! Ends the running action and drops the plan.
        void Abort(const ActionStateRegistry& registry, ActionContext& context);

        //! True when a plan is loaded and has steps left.
        bool HasPlan() const;

        //! The action currently running, or nullptr when there is none.
        const ActionRequest* GetCurrentAction() const;

        //! Seconds the current action has been running.
        float GetElapsed() const { return m_elapsed; }

        //! Index of the step being run, for console output.
        size_t GetStepIndex() const { return m_step; }

    private:
        //! Ends the running action if one has begun, leaving the plan alone.
        void EndCurrent(const ActionStateRegistry& registry, ActionContext& context);

        //! Points the context at this agent's current action and scratch.
        void FillContext(ActionContext& context) const;

        //! The store the current plan's steps live in, so they can be given back without the
        //! caller having to remember which store issued them.
        PlanStore* m_store = nullptr;
        ActionPlan m_plan;
        mutable ActionScratch m_scratch{};
        size_t m_step = 0;
        float m_elapsed = 0.0f;
        bool m_begun = false;
    };
} // namespace GOAT
