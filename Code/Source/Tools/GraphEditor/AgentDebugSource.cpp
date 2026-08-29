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
        return m_playing && AgentSystemInterface::Get() != nullptr;
    }

    AZStd::string LocalAgentDebugSource::DescribeTarget() const
    {
        if (AgentSystemInterface::Get() == nullptr)
        {
            return "the agent system is not running";
        }
        if (!m_playing)
        {
            return "not playing - press Ctrl+G to run the game in the editor";
        }
        return AZStd::string::format("this editor, %zu agent(s)", m_snapshots.size());
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
        m_snapshots.clear();
    }
} // namespace GOAT::GraphEditor
