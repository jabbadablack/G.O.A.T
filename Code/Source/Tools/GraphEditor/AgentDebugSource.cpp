#include <Tools/GraphEditor/AgentDebugSource.h>

#include <GOAT/GOATRemoteDebug.h>
#include <GOAT/Interfaces/IAgentSystem.h>

namespace GOAT::GraphEditor
{
    LocalAgentDebugSource::LocalAgentDebugSource()
    {
        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusConnect();
    }

    LocalAgentDebugSource::~LocalAgentDebugSource()
    {
        AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusDisconnect();
    }

    bool LocalAgentDebugSource::IsConnected() const
    {
        return AgentSystemInterface::Get() != nullptr;
    }

    AZStd::string LocalAgentDebugSource::DescribeTarget() const
    {
        if (AgentSystemInterface::Get() == nullptr)
        {
            return "The agent system is not running.";
        }
        if (m_snapshots.empty())
        {
            return m_playing ? "No agents are registered."
                             : "No agents are registered. Open a level, or press Ctrl+G to play.";
        }
        return AZStd::string::format(
            "this editor%s, %zu agent(s)", m_playing ? " (playing)" : "", m_snapshots.size());
    }

    void LocalAgentDebugSource::Poll()
    {
        if (!IsConnected())
        {
            m_snapshots.clear();
            return;
        }
        m_snapshots = AgentSystemInterface::Get()->SnapshotAgents(m_watched);
    }

    void LocalAgentDebugSource::OnStartPlayInEditor()
    {
        m_playing = true;
    }

    void LocalAgentDebugSource::OnStopPlayInEditor()
    {
        m_playing = false;
    }
    RemoteAgentDebugSource::RemoteAgentDebugSource() = default;

    RemoteAgentDebugSource::~RemoteAgentDebugSource() = default;

    bool RemoteAgentDebugSource::IsConnected() const
    {
#if defined(ENABLE_REMOTE_TOOLS)
        auto* remoteTools = AzFramework::RemoteToolsInterface::Get();
        if (remoteTools == nullptr)
        {
            return false;
        }

        const AzFramework::RemoteToolsEndpointInfo target = remoteTools->GetDesiredEndpoint(GoatToolsKey);
        return target.IsValid() && !target.IsSelf() &&
            remoteTools->IsEndpointOnline(GoatToolsKey, target.GetNetworkId());
#else
        return false;
#endif
    }

    AZStd::string RemoteAgentDebugSource::DescribeTarget() const
    {
#if defined(ENABLE_REMOTE_TOOLS)
        if (AzFramework::RemoteToolsInterface::Get() == nullptr)
        {
            return "Remote debugging needs the RemoteTools gem enabled.";
        }
        if (!m_trouble.empty())
        {
            return m_trouble;
        }
        if (!IsConnected())
        {
            return "No launcher is attached. Start one in profile or debug -- a release build "
                   "carries no remote tools service.";
        }

        const AzFramework::RemoteToolsEndpointInfo target =
            AzFramework::RemoteToolsInterface::Get()->GetDesiredEndpoint(GoatToolsKey);
        return AZStd::string::format("%s, %zu agent(s)", target.GetDisplayName(), m_snapshots.size());
#else
        return "This build carries no remote tools service.";
#endif
    }

    bool RemoteAgentDebugSource::AttachToFirstTarget()
    {
#if defined(ENABLE_REMOTE_TOOLS)
        auto* remoteTools = AzFramework::RemoteToolsInterface::Get();
        if (remoteTools == nullptr)
        {
            return false;
        }

        AzFramework::RemoteToolsEndpointContainer targets;
        remoteTools->EnumTargetInfos(GoatToolsKey, targets);
        for (const auto& [id, info] : targets)
        {
            if (!info.IsSelf() && info.IsOnline())
            {
                // Without this the host discards everything the launcher sends, silently.
                remoteTools->SetDesiredEndpoint(GoatToolsKey, info.GetNetworkId());
                m_trouble.clear();
                return true;
            }
        }
#endif
        return false;
    }

    void RemoteAgentDebugSource::Detach()
    {
#if defined(ENABLE_REMOTE_TOOLS)
        if (auto* remoteTools = AzFramework::RemoteToolsInterface::Get())
        {
            remoteTools->SetDesiredEndpoint(GoatToolsKey, 0);
        }
#endif
        m_snapshots.clear();
        m_trouble.clear();
    }

    void RemoteAgentDebugSource::Poll()
    {
#if defined(ENABLE_REMOTE_TOOLS)
        auto* remoteTools = AzFramework::RemoteToolsInterface::Get();
        if (remoteTools == nullptr)
        {
            return;
        }

        if (const auto* messages = remoteTools->GetReceivedMessages(GoatToolsKey); messages != nullptr)
        {
            for (const AzFramework::RemoteToolsMessagePointer& message : *messages)
            {
                const auto* reply = azrtti_cast<const GOATDebugReply*>(message.get());
                if (reply == nullptr)
                {
                    continue;
                }
                if (reply->m_protocolVersion != GoatDebugProtocolVersion)
                {
                    m_trouble = AZStd::string::format(
                        "That launcher speaks protocol %u and this editor speaks %u. Rebuild both.",
                        reply->m_protocolVersion, GoatDebugProtocolVersion);
                    m_snapshots.clear();
                    continue;
                }
                m_trouble.clear();
                m_snapshots = reply->m_agents;
            }
            remoteTools->ClearReceivedMessages(GoatToolsKey);
        }

        if (!IsConnected())
        {
            m_snapshots.clear();
            return;
        }

        // One ask per poll. The transport's inbox holds 128 messages and does not check for
        // overflow on the network path, so a burst is not something to risk.
        const AzFramework::RemoteToolsEndpointInfo target = remoteTools->GetDesiredEndpoint(GoatToolsKey);

        GOATDebugRequest request;
        request.SetWatched(m_watched);
        remoteTools->SendRemoteToolsMessage(target, request);
#endif
    }
} // namespace GOAT::GraphEditor
