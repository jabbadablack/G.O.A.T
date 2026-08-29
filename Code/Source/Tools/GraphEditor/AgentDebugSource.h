#pragma once

#include <GOAT/Domain/AgentDebug.h>

#include <AzToolsFramework/Entity/EditorEntityContextBus.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT::GraphEditor
{
    //! Where a tool reads agent state from.
    //!
    //! One implementation reads the agent system in this process, another asks a launcher over
    //! the network. The panels are written against this so that adding the second one is a
    //! transport rather than a second copy of the tool.
    class IAgentDebugSource
    {
    public:
        virtual ~IAgentDebugSource() = default;

        //! True when there is something to read. False is a normal state, not a fault: in the
        //! editor it means the game is not running.
        virtual bool IsConnected() const = 0;

        //! What this is watching, for the status bar. Says why it is not connected when it
        //! is not, because "no agents" and "nowhere to look" are different problems.
        virtual AZStd::string DescribeTarget() const = 0;

        //! Asks for fresh state. A source that answers over a wire only sends the request here.
        virtual void Poll() = 0;

        //! What the last poll produced. Empty until one answers.
        virtual const AZStd::vector<AgentSnapshot>& GetSnapshots() const = 0;
    };

    //! Reads the agent system running in this process.
    //!
    //! Agents turn out to exist in edit mode as well as in game mode, so this reports whatever
    //! the agent system holds and uses the play state only to explain an empty list. Gating on
    //! game mode would have hidden a level's agents from the tool for no reason.
    class LocalAgentDebugSource final
        : public IAgentDebugSource
        , private AzToolsFramework::EditorEntityContextNotificationBus::Handler
    {
    public:
        LocalAgentDebugSource();
        ~LocalAgentDebugSource() override;

        bool IsConnected() const override;
        AZStd::string DescribeTarget() const override;
        void Poll() override;
        const AZStd::vector<AgentSnapshot>& GetSnapshots() const override { return m_snapshots; }

    private:
        // EditorEntityContextNotificationBus
        void OnStartPlayInEditor() override;
        void OnStopPlayInEditor() override;

        AZStd::vector<AgentSnapshot> m_snapshots;
        //! Only used to say why a list is empty, never to decide whether to read.
        bool m_playing = false;
    };

    //! Reads the agents of a launcher running beside the editor.
    //!
    //! The editor is the host and the launcher dials in, so a game can be started and stopped
    //! as often as you like while this window stays open.
    class RemoteAgentDebugSource final
        : public IAgentDebugSource
    {
    public:
        RemoteAgentDebugSource();
        ~RemoteAgentDebugSource() override;

        bool IsConnected() const override;
        AZStd::string DescribeTarget() const override;
        void Poll() override;
        const AZStd::vector<AgentSnapshot>& GetSnapshots() const override { return m_snapshots; }

        //! Attaches to the first launcher that is not this process. False when none has dialled
        //! in yet, which is the normal state until one is started.
        bool AttachToFirstTarget();

        //! Stops listening to whatever it was attached to.
        void Detach();

    private:
        AZStd::vector<AgentSnapshot> m_snapshots;
        //! Why the last poll produced nothing, when something went wrong rather than the
        //! launcher simply not being there.
        AZStd::string m_trouble;
    };
} // namespace GOAT::GraphEditor
