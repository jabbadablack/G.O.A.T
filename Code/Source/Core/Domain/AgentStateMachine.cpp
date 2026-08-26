#include <GOAT/Domain/AgentStateMachine.h>

#include <Core/Application/ActionStateRegistry.h>

namespace GOAT
{
    void AgentStateMachine::SetPlan(const ActionPlan& plan)
    {
        m_plan = plan;
        m_step = 0;
        m_elapsed = 0.0f;
        m_begun = false;
        m_scratch.fill(0);
    }

    bool AgentStateMachine::HasPlan() const
    {
        return m_step < m_plan.m_steps.size();
    }

    const ActionRequest* AgentStateMachine::GetCurrentAction() const
    {
        return HasPlan() ? &m_plan.m_steps[m_step] : nullptr;
    }

    void AgentStateMachine::FillContext(ActionContext& context) const
    {
        context.m_request = GetCurrentAction();
        context.m_scratch = &m_scratch;
    }

    void AgentStateMachine::EndCurrent(const ActionStateRegistry& registry, ActionContext& context)
    {
        if (!m_begun)
        {
            return;
        }

        FillContext(context);
        if (const ActionRequest* request = GetCurrentAction())
        {
            if (IActionState* state = registry.Find(request->m_action))
            {
                state->End(context);
            }
        }
        m_begun = false;
    }

    ActionResult AgentStateMachine::Step(const ActionStateRegistry& registry, ActionContext& context, float deltaTime)
    {
        if (!HasPlan())
        {
            return ActionResult::Success;
        }

        FillContext(context);

        IActionState* state = registry.Find(m_plan.m_steps[m_step].m_action);
        if (state == nullptr)
        {
            // The verb's module was removed, so the plan can no longer be run.
            AZ_Warning(
                "GOAT", false, "Agent is running unregistered action verb %u; failing the plan",
                static_cast<AZ::u32>(m_plan.m_steps[m_step].m_action));
            m_begun = false;
            m_step = m_plan.m_steps.size();
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

        if (result == ActionResult::Failure)
        {
            // A failed step ends the whole plan; the tree decides what to do next.
            m_step = m_plan.m_steps.size();
            return ActionResult::Failure;
        }

        ++m_step;
        return HasPlan() ? ActionResult::Running : ActionResult::Success;
    }

    void AgentStateMachine::Abort(const ActionStateRegistry& registry, ActionContext& context)
    {
        EndCurrent(registry, context);
        m_step = m_plan.m_steps.size();
        m_elapsed = 0.0f;
    }
} // namespace GOAT
