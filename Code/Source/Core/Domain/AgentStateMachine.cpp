#include <GOAT/Domain/AgentStateMachine.h>

#include <Core/Application/ActionStateRegistry.h>

namespace GOAT
{
    void AgentStateMachine::ReleasePlan()
    {
        // A baked plan's span is shared and carries no block, so Release ignores it; only a
        // computed plan actually gives anything back here.
        if (m_store != nullptr)
        {
            m_store->Release(m_plan.m_span);
        }

        m_plan = ActionPlan{};
        m_store = nullptr;
        m_step = 0;

        AZ_Assert(!HasPlan(), "Releasing a plan must leave the machine with nothing to run");
    }

    void AgentStateMachine::SetPlan(PlanStore& store, const ActionPlan& plan)
    {
        AZ_Assert(!plan.IsEmpty(), "A state machine is only ever given a plan with steps in it");

        // The outgoing plan may have borrowed a block; taking a new one is where it goes back.
        ReleasePlan();

        m_store = &store;
        m_plan = plan;
        m_step = 0;
        m_elapsed = 0.0f;
        m_begun = false;
        m_scratch.fill(0);

        AZ_Assert(HasPlan(), "Setting a non empty plan must leave the machine with work to do");
        AZ_Assert(!m_begun, "A fresh plan has not begun its first action yet");
    }

    bool AgentStateMachine::HasPlan() const
    {
        return m_step < m_plan.Size();
    }

    const ActionRequest* AgentStateMachine::GetCurrentAction() const
    {
        return HasPlan() ? m_plan.GetStep(m_step) : nullptr;
    }

    void AgentStateMachine::FillContext(ActionContext& context, WakeCondition* wake) const
    {
        context.m_request = GetCurrentAction();
        context.m_scratch = &m_scratch;
        context.m_wake = wake;

        AZ_Assert(context.m_scratch != nullptr, "Every action runs with scratch it owns for its whole lifetime");
    }

    void AgentStateMachine::EndCurrent(const ActionStateRegistry& registry, ActionContext& context)
    {
        if (!m_begun)
        {
            return;
        }

        WakeCondition discarded;
        FillContext(context, &discarded);
        if (const ActionRequest* request = GetCurrentAction())
        {
            IActionState* state = registry.Find(request->m_action);
            AZ_Warning("GOAT", state != nullptr,
                "Verb %u was unregistered while an agent was running it, so its End was never called",
                static_cast<AZ::u32>(request->m_action));

            if (state != nullptr)
            {
                state->End(context);
            }
        }
        m_begun = false;

        AZ_Assert(!m_begun, "Ending the current action must leave nothing begun");
    }

    ActionResult AgentStateMachine::Step(
        const ActionStateRegistry& registry, ActionContext& context, float deltaTime, WakeCondition& outWake)
    {
        AZ_Assert(deltaTime >= 0.0f, "A plan cannot be stepped backwards in time");
        AZ_Assert(!m_begun || HasPlan(), "An action cannot be running while the plan has no current step");

        if (!HasPlan())
        {
            return ActionResult::Success;
        }

        outWake = WakeCondition{};
        FillContext(context, &outWake);
        AZ_Assert(context.m_request != nullptr, "A machine with a plan always has a current action to run");

        const ActionRequest* current = m_plan.GetStep(m_step);
        AZ_Assert(current != nullptr, "A machine with a plan always has a current step");

        IActionState* state = registry.Find(current->m_action);
        if (state == nullptr)
        {
            // The verb's module was removed, so the plan can no longer be run.
            AZ_Warning(
                "GOAT", false, "Agent is running unregistered action verb %u; failing the plan",
                static_cast<AZ::u32>(current->m_action));
            m_begun = false;
            m_step = m_plan.Size();
            return ActionResult::Failure;
        }

        if (!m_begun)
        {
            m_scratch.fill(0);
            m_elapsed = 0.0f;
            state->Begin(context);
            m_begun = true;
        }

        m_elapsed += deltaTime;
        const ActionResult result = state->Step(context, deltaTime);
        if (result == ActionResult::Running)
        {
            return ActionResult::Running;
        }

        state->End(context);
        m_begun = false;

        AZ_Assert(result == ActionResult::Success || result == ActionResult::Failure,
            "A finished action reports success or failure, never running");

        if (result == ActionResult::Failure)
        {
            // A failed step ends the whole plan; the tree decides what to do next.
            m_step = m_plan.Size();
            return ActionResult::Failure;
        }

        ++m_step;
        return HasPlan() ? ActionResult::Running : ActionResult::Success;
    }

    void AgentStateMachine::Abort(const ActionStateRegistry& registry, ActionContext& context)
    {
        // Ending first is what lets an action release whatever it borrowed, such as a path slot.
        EndCurrent(registry, context);
        m_step = m_plan.Size();
        m_elapsed = 0.0f;

        // Aborting drops the plan for good, so whatever it borrowed goes back here.
        ReleasePlan();

        AZ_Assert(!HasPlan(), "Aborting must leave the machine with no plan to continue");
        AZ_Assert(!m_begun, "Aborting must leave no action begun");
    }
} // namespace GOAT
