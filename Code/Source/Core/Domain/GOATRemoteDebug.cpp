#include <GOAT/GOATRemoteDebug.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void GOATDebugRequest::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOATDebugRequest, AzFramework::RemoteToolsMessage>()
                ->Version(1)
                ->Field("ProtocolVersion", &GOATDebugRequest::m_protocolVersion)
                ->Field("WatchedIndex", &GOATDebugRequest::m_watchedIndex)
                ->Field("WatchedGeneration", &GOATDebugRequest::m_watchedGeneration);
        }
    }

    void GOATDebugReply::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOATDebugReply, AzFramework::RemoteToolsMessage>()
                ->Version(1)
                ->Field("ProtocolVersion", &GOATDebugReply::m_protocolVersion)
                ->Field("Agents", &GOATDebugReply::m_agents);
        }
    }

    void ReflectRemoteDebug(AZ::ReflectContext* context)
    {
        GOATDebugRequest::Reflect(context);
        GOATDebugReply::Reflect(context);
    }
} // namespace GOAT
