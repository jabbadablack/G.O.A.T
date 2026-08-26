#include <GOAT/Domain/Guard.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void Guard::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<Guard>()
                ->Version(1)
                ->Field("Key", &Guard::m_key)
                ->Field("Node", &Guard::m_node)
                ->Field("Abort", &Guard::m_abort);
        }
    }

    void ReflectGuardTypes(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Enum<AbortMode>()
                ->Value("None", AbortMode::None)
                ->Value("Self", AbortMode::Self)
                ->Value("LowerPriority", AbortMode::LowerPriority)
                ->Value("Both", AbortMode::Both);
        }

        Guard::Reflect(context);
    }
} // namespace GOAT
