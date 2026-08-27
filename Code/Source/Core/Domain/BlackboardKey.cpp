#include <GOAT/Domain/BlackboardKey.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void BlackboardKey::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BlackboardKey>()->Version(1)->Field("Packed", &BlackboardKey::m_packed);
        }
    }
} // namespace GOAT
