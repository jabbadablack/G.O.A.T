#include <Tools/GraphEditor/AgentDebugSource.h>

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
        m_snapshots = AgentSystemInterface::Get()->SnapshotAgents();
    }

    void LocalAgentDebugSource::OnStartPlayInEditor()
    {
        m_playing = true;
    }

    void LocalAgentDebugSource::OnStopPlayInEditor()
    {
        m_playing = false;
    }
} // namespace GOAT::GraphEditor
