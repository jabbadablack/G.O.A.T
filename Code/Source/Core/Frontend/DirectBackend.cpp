#include <Core/Frontend/DirectBackend.h>

#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    AZ::Name DirectBackend::GetBackendName()
    {
        return AZ_NAME_LITERAL("direct");
    }

    AZ::Name DirectBackend::GetName() const
    {
        return GetBackendName();
    }

    bool DirectBackend::Plan(
        [[maybe_unused]] const PlanContext& context, const Intent& intent, ActionPlan& outPlan)
    {
        AZ_Assert(intent.m_backend.IsEmpty() || intent.m_backend == GetBackendName(),
            "The direct backend was handed an intent addressed to '%s'", intent.m_backend.GetCStr());

        if (intent.m_direct.m_action == CoreActions::Invalid)
        {
            AZ_Error("GOAT", false,
                "The direct backend cannot plan node %u: its leaf named no registered verb", intent.m_node);
            return false;
        }

        outPlan.m_steps.clear();
        outPlan.m_steps.push_back(intent.m_direct);

        AZ_Assert(outPlan.m_steps.size() == 1, "A direct plan is exactly the one action the leaf asked for");
        return true;
    }
} // namespace GOAT
