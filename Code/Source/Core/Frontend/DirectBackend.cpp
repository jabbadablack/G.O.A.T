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
        if (intent.m_direct.m_action == CoreActions::Invalid)
        {
            return false;
        }

        outPlan.m_steps.clear();
        outPlan.m_steps.push_back(intent.m_direct);
        return true;
    }
} // namespace GOAT
