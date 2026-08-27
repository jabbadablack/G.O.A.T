#include <GOAT/Domain/Intent.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void Intent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Intent>()
                ->Version(1)
                ->Field("Backend", &Intent::m_backend)
                ->Field("Goal", &Intent::m_goal)
                ->Field("Node", &Intent::m_node);
        }
    }
} // namespace GOAT
