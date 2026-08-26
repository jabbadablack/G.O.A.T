#pragma once

#include <Core/Application/ActionStateRegistry.h>
#include <Core/Application/AgentRegistry.h>
#include <Core/Application/AgentRuntime.h>
#include <Core/Application/BackendRegistry.h>
#include <Core/Application/BlackboardSystem.h>
#include <Core/Application/NodeTypeRegistry.h>
#include <Core/Frontend/TreeLibrary.h>
#include <Core/Scripting/AgentScriptContext.h>
#include <Core/Scripting/LuaDispatch.h>
#include <Core/Scripting/LuaNodeScripting.h>

#include <GOAT/GOATBus.h>
#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzFramework/Asset/AssetCatalogBus.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! Owns every GOAT service and registers them for the lifetime of the gem.
    //! The editor system component derives from this, so registering here covers both modules.
    class GOATSystemComponent
        : public AZ::Component
        , public IAgentSystem
        , protected GOATRequestBus::Handler
        , protected AzFramework::AssetCatalogEventBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(GOATSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        GOATSystemComponent();
        ~GOATSystemComponent();

        ////////////////////////////////////////////////////////////////////////
        // IAgentSystem
        bool LoadScript(const AZ::Data::Asset<AZ::ScriptAsset>& asset) override;
        AZ::Outcome<void, AZStd::string> LoadBlackboard(const BlackboardAsset& asset) override;
        AZ::Outcome<void, AZStd::string> CompileTree(const AZ::Name& treeName) override;
        AgentId RegisterAgent(AZ::EntityId entity, const AZ::Name& treeName, size_t band) override;
        void UnregisterAgent(AgentId agent) override;
        void JoinSquad(AgentId agent, const AZ::Name& squad) override;
        bool RegisterBackend(AZStd::unique_ptr<IBackend> backend) override;
        void UnregisterBackend(const AZ::Name& name) override;
        ActionStateId RegisterAction(AZStd::unique_ptr<IActionState> action) override;
        void UnregisterAction(ActionStateId id) override;
        AZStd::vector<AZ::Name> GetBackendNames() const override;
        AZStd::vector<AZ::Name> GetActionNames() const override;
        AZStd::vector<AZ::Name> GetTreeNames() const override;
        AZStd::string DescribeAgent(AgentId agent) const override;
        ////////////////////////////////////////////////////////////////////////

    protected:
        ////////////////////////////////////////////////////////////////////////
        // AzFramework::AssetCatalogEventBus
        //! System components activate before the asset catalog loads, so the vocabulary is
        //! picked up here rather than at activation.
        void OnCatalogLoaded(const char* catalogFile) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // Console commands. AZ::IConsole is the engine's only command registry.
        void ListBackends(const AZ::ConsoleCommandContainer& arguments);
        void ListActions(const AZ::ConsoleCommandContainer& arguments);
        void ListNodes(const AZ::ConsoleCommandContainer& arguments);
        void ListTrees(const AZ::ConsoleCommandContainer& arguments);
        void ListAgents(const AZ::ConsoleCommandContainer& arguments);
        void DumpAgent(const AZ::ConsoleCommandContainer& arguments);
        void ReloadVocabulary(const AZ::ConsoleCommandContainer& arguments);

        AZ_CONSOLEFUNC(GOATSystemComponent, ListBackends, AZ::ConsoleFunctorFlags::Null,
            "Lists the decision backends currently installed");
        AZ_CONSOLEFUNC(GOATSystemComponent, ListActions, AZ::ConsoleFunctorFlags::Null,
            "Lists the action verbs currently registered");
        AZ_CONSOLEFUNC(GOATSystemComponent, ListNodes, AZ::ConsoleFunctorFlags::Null,
            "Lists the node types trees may use");
        AZ_CONSOLEFUNC(GOATSystemComponent, ListTrees, AZ::ConsoleFunctorFlags::Null,
            "Lists the trees compiled so far");
        AZ_CONSOLEFUNC(GOATSystemComponent, ListAgents, AZ::ConsoleFunctorFlags::Null,
            "Lists every running agent");
        AZ_CONSOLEFUNC(GOATSystemComponent, DumpAgent, AZ::ConsoleFunctorFlags::Null,
            "Prints what one agent is doing, by entity id");
        AZ_CONSOLEFUNC(GOATSystemComponent, ReloadVocabulary, AZ::ConsoleFunctorFlags::Null,
            "Reloads the GOAT Lua vocabulary and reports where it was found");
        ////////////////////////////////////////////////////////////////////////

    private:
        //! Registers the asset handlers this gem owns. Safe when another module already did.
        void RegisterAssetHandlers();
        void UnregisterAssetHandlers();

        //! Brings up the pipeline: registries, the direct backend, the core verbs and Lua.
        void StartServices();
        void StopServices();

        //! Loads the Lua authoring vocabulary shipped with the gem.
        bool LoadVocabulary();

        //! Loads the vocabulary if it is not loaded yet.
        //! Activation can run before the asset catalog is ready, so this retries on first use
        //! rather than leaving the gem permanently unable to compile a tree.
        bool EnsureVocabulary();

        //! Installs a backend front for every backend a script declared in Lua.
        void RegisterLuaBackends();

        AZStd::unique_ptr<BlackboardSystem> m_blackboardSystem;
        AZStd::unique_ptr<ActionStateRegistry> m_actions;
        AZStd::unique_ptr<BackendRegistry> m_backends;
        AZStd::unique_ptr<NodeTypeRegistry> m_nodeTypes;
        AZStd::unique_ptr<TreeLibrary> m_trees;
        AZStd::unique_ptr<LuaDispatch> m_dispatch;
        //! Stable for the gem's lifetime, because Lua receives a raw pointer to it.
        AZStd::unique_ptr<AgentScriptContext> m_scriptContext;
        AZStd::unique_ptr<LuaNodeScripting> m_scripting;
        AZStd::unique_ptr<IBackend> m_directBackend;
        AZStd::unique_ptr<AgentRuntime> m_runtime;
        AZStd::unique_ptr<AgentRegistry> m_agents;

        //! Trees compiled so far, shared by every agent running the same one.
        AZStd::unordered_map<AZ::Name, AZStd::shared_ptr<const DecisionProgram>> m_programs;
        AZStd::vector<AZStd::unique_ptr<AZ::Data::AssetHandler>> m_assetHandlers;
        //! Whether the authoring vocabulary is loaded into the script context.
        bool m_vocabularyLoaded = false;
    };
} // namespace GOAT
