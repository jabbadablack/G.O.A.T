#pragma once

#include <Core/Application/ActionStateRegistry.h>
#include <Core/Application/BackendRegistry.h>
#include <Core/Application/NodeTypeRegistry.h>

#include <GOAT/Interfaces/IAgentSystem.h>

namespace GOAT
{
    //! The core as a backend sees it, with nothing behind it but registries a test built.
    //! Only the five a backend actually reaches do anything; the rest answer emptily.
    class TestAgentSystem final
        : public IAgentSystem
    {
    public:
        TestAgentSystem(
            const NodeTypeRegistry& nodeTypes, const ActionStateRegistry& actions,
            const BackendRegistry* backends = nullptr)
            : m_nodeTypes(nodeTypes)
            , m_actions(actions)
            , m_backends(backends)
        {
        }

        AZ::Outcome<AZStd::shared_ptr<const AuthoredNode>, AZStd::string> EmitProgram(const AZ::Name&) override
        {
            return AZ::Failure(AZStd::string("a test declares its programs in C++"));
        }

        AZ::Name GetSubtreeBinding(const AZ::Name&) const override { return {}; }

        ActionStateId FindVerb(const AZ::Name& name) const override { return m_actions.FindId(name); }

        const NodeTypeDescriptor* FindNodeType(const AZ::Name& name) const override
        {
            return m_nodeTypes.Find(name);
        }

        IBackend* FindBackend(const AZ::Name& name) const override
        {
            return m_backends != nullptr ? m_backends->Find(name) : nullptr;
        }

        ActionResult CallBehavior(const AZ::Name&, const char*, AgentId, float) override
        {
            ++m_behaviourCalls;
            return ActionResult::Success;
        }

        int m_behaviourCalls = 0;

        ////////////////////////////////////////////////////////////////////////
        // Everything else a game would use and a backend never does.
        bool LoadScript(const AZ::Data::Asset<AZ::ScriptAsset>&) override { return false; }
        AZ::Outcome<void, AZStd::string> LoadBlackboard(const BlackboardAsset&) override { return AZ::Success(); }
        AZ::Outcome<void, AZStd::string> CompileProgram(const AZ::Name&, const AZ::Name&) override
        {
            return AZ::Success();
        }
        bool IsProgramCompiled(const AZ::Name&) const override { return false; }
        AgentId RegisterAgent(
            AZ::EntityId, const AZ::Name&, AZStd::span<const AZ::Name>, size_t, const AZ::Name&) override
        {
            return AgentId{};
        }
        void UnregisterAgent(AgentId) override {}
        bool SetAgentTree(AgentId, const AZ::Name&, AZ::u8) override { return false; }
        bool PushAgentTree(AgentId, const AZ::Name&, AZ::u8) override { return false; }
        bool PopAgentTree(AgentId) override { return false; }
        AZ::Name GetAgentTree(AgentId) const override { return {}; }
        void JoinSquad(AgentId, const AZ::Name&) override {}
        void LeaveSquad(AgentId) override {}
        AZ::Name GetAgentSquad(AgentId) const override { return {}; }
        AZStd::vector<AgentId> GetAgents() const override { return {}; }
        AgentId FindAgent(AZ::EntityId) const override { return AgentId{}; }
        AZ::EntityId GetAgentEntity(AgentId) const override { return AZ::EntityId{}; }
        bool SetAgentBand(AgentId, size_t) override { return false; }
        size_t GetAgentBand(AgentId) const override { return 0; }
        AZ::Outcome<size_t, AZStd::string> RebindSubtree(const AZ::Name&, const AZ::Name&) override
        {
            return AZ::Success(size_t{ 0 });
        }
        bool RegisterDirector(AgentId, const DirectorProfile&) override { return false; }
        void UnregisterDirector(AgentId) override {}
        size_t GetReachSize(AgentId) override { return 0; }
        AgentId GetInReach(AgentId, size_t) override { return AgentId{}; }
        bool RegisterReachFilter(AZStd::unique_ptr<IReachFilter>) override { return false; }
        void UnregisterReachFilter(const AZ::Name&) override {}
        AZStd::vector<AZ::Name> GetReachFilterNames() const override { return {}; }
        void WakeAgents(AZStd::span<const AgentId>) override {}
        bool RegisterBackend(AZStd::unique_ptr<IBackend>) override { return false; }
        void UnregisterBackend(const AZ::Name&) override {}
        bool RegisterNodeType(NodeTypeDescriptor) override { return false; }
        void UnregisterNodeType(const AZ::Name&) override {}
        ActionStateId RegisterAction(AZStd::unique_ptr<IActionState>) override { return CoreActions::Invalid; }
        void UnregisterAction(ActionStateId) override {}
        AZStd::vector<AZ::Name> GetBackendNames() const override { return {}; }
        AZStd::vector<AZ::Name> GetActionNames() const override { return {}; }
        AZStd::vector<AZ::Name> GetTreeNames() const override { return {}; }
        AZStd::vector<AZ::Name> GetNodeTypeNames() const override { return {}; }
        AZStd::string DescribeAgent(AgentId) const override { return {}; }
        ////////////////////////////////////////////////////////////////////////

    private:
        const NodeTypeRegistry& m_nodeTypes;
        const ActionStateRegistry& m_actions;
        const BackendRegistry* m_backends = nullptr;
    };
} // namespace GOAT
