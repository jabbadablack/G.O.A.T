#include <Core/Actions/WaitAction.h>

#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    namespace
    {
        //! Seconds this agent has waited so far, kept in the agent's scratch.
        float& Elapsed(const ActionContext& context)
        {
            return *reinterpret_cast<float*>(context.m_scratch->data());
        }

        static_assert(sizeof(float) <= AZStd::tuple_size<ActionScratch>::value, "Wait state does not fit in the scratch");
    } // namespace

    AZ::Name WaitAction::GetName() const
    {
        return AZ_NAME_LITERAL("wait");
    }

    void WaitAction::Begin(const ActionContext& context)
    {
        Elapsed(context) = 0.0f;
    }

    ActionResult WaitAction::Step(const ActionContext& context, float deltaTime)
    {
        float& elapsed = Elapsed(context);
        elapsed += deltaTime;
        return elapsed >= context.m_request->m_duration ? ActionResult::Success : ActionResult::Running;
    }

    void WaitAction::End([[maybe_unused]] const ActionContext& context)
    {
    }
} // namespace GOAT
