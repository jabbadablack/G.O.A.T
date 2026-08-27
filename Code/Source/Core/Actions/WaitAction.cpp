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
        AZ_Assert(context.m_scratch != nullptr, "A wait always runs with agent scratch to count in");
        AZ_Assert(context.m_request != nullptr, "A wait always runs with a request naming its duration");
        AZ_Warning("GOAT", context.m_request == nullptr || context.m_request->m_amount >= 0.0f,
            "A wait was given a negative duration, so it will finish immediately");

        Elapsed(context) = 0.0f;
    }

    ActionResult WaitAction::Step(const ActionContext& context, float deltaTime)
    {
        AZ_Assert(deltaTime >= 0.0f, "A wait cannot be stepped backwards in time");

        float& elapsed = Elapsed(context);
        elapsed += deltaTime;

        AZ_Assert(elapsed >= 0.0f, "Elapsed wait time must never go negative");
        return elapsed >= context.m_request->m_amount ? ActionResult::Success : ActionResult::Running;
    }

} // namespace GOAT
