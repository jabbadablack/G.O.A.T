#include <GOAT/Domain/ActionPlan.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void ActionPlan::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ActionPlan>()->Version(1)->Field("Steps", &ActionPlan::m_steps);
        }
    }
} // namespace GOAT
