#pragma once

#include <GOAT/Domain/AgentDebug.h>

#include <AzFramework/Network/IRemoteTools.h>

#include <AzCore/Math/Crc.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    //! The service the editor listens on and a launcher dials into.
    //!
    //! A (name, key, port) triple in the shape AzFramework declares for Lua and ScriptCanvas.
    //!
    //! The port has to be free of every other service on the machine, not just the other tools:
    //! 6777 is the Lua debugger, 45641 is ScriptCanvas, and 45643 is the **Asset Processor**
    //! (`Registry/bootstrap.setreg`, `remote_port`). Listening on that last one makes the editor
    //! hang at startup waiting for an Asset Processor it has just taken the port of.
    //! @{
    static const AZ::Name GoatToolsName = AZ::Name::FromStringLiteral("GoatRemoteTools", nullptr);
    static constexpr AZ::Crc32 GoatToolsKey("GoatRemoteTools");
    static constexpr uint16_t GoatToolsPort = 45647;
    //! @}

    //! Bumped whenever a message changes shape.
    //!
    //! The transport has no version of its own and deserializes an unknown class to null rather
    //! than complaining, so without this an editor and a launcher built from different sources
    //! would quietly exchange nothing at all and look merely idle.
    inline constexpr AZ::u32 GoatDebugProtocolVersion = 1;

    //! Editor to launcher: send what your agents are doing.
    class GOATDebugRequest final
        : public AzFramework::RemoteToolsMessage
    {
    public:
        AZ_CLASS_ALLOCATOR(GOATDebugRequest, AZ::SystemAllocator);
        AZ_RTTI(GOATDebugRequest, "{5DE833C5-FC01-4A11-B17C-D3B33464D393}", AzFramework::RemoteToolsMessage);

        static void Reflect(AZ::ReflectContext* context);

        AZ::u32 m_protocolVersion = GoatDebugProtocolVersion;

        //! The agent the tool is actually looking at, so the launcher works out where that one
        //! is inside its program and does not pay to do it for every agent in the level.
        //! @{
        AZ::u32 m_watchedIndex = AgentId::NullIndex;
        AZ::u32 m_watchedGeneration = 0;

        AgentId GetWatched() const { return AgentId(m_watchedIndex, m_watchedGeneration); }
        void SetWatched(AgentId agent)
        {
            m_watchedIndex = agent.GetIndex();
            m_watchedGeneration = agent.GetGeneration();
        }
        //! @}
    };

    //! Launcher to editor: this is what they are doing.
    class GOATDebugReply final
        : public AzFramework::RemoteToolsMessage
    {
    public:
        AZ_CLASS_ALLOCATOR(GOATDebugReply, AZ::SystemAllocator);
        AZ_RTTI(GOATDebugReply, "{BC54273F-1482-4831-A8C0-DDAD4E55CB7C}", AzFramework::RemoteToolsMessage);

        static void Reflect(AZ::ReflectContext* context);

        AZ::u32 m_protocolVersion = GoatDebugProtocolVersion;
        AZStd::vector<AgentSnapshot> m_agents;
    };

    //! Reflects both messages. Called from both modules, because each side has to know the
    //! shape of what the other sends.
    void ReflectRemoteDebug(AZ::ReflectContext* context);
} // namespace GOAT
