#pragma once

#include <Core/Application/ActionStateRegistry.h>
#include <Core/Application/AgentArchetype.h>
#include <Core/Application/AgentRegistry.h>
#include <Core/Application/AgentRuntime.h>
#include <Core/Application/BackendRegistry.h>
#include <Backends/Htn/HtnBackend.h>
#include <Core/Application/DecisionBackendRegistry.h>
#include <Core/Application/BlackboardSystem.h>
#include <Core/Application/NodeTypeRegistry.h>
#include <Core/Application/ReachFilterRegistry.h>
#include <Core/Director/DirectorKeys.h>
#include <Core/Director/DirectorRegistry.h>
#include <Core/Frontend/TreeLibrary.h>
#include <Core/Scripting/AgentScriptContext.h>
#include <Core/Scripting/LuaDispatch.h>
#include <Core/Scripting/LuaNodeScripting.h>

#include <GOAT/GOATBackendBus.h>
#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzFramework/Asset/AssetCatalogBus.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! Owns every GOAT service and registers them for the lifetime of the gem.
    //! The editor system component derives from this, so registering here covers both modules.
    class GOATSystemComponent
        : public AZ::Component
        , public IAgentSystem
        , protected AzFramework::AssetCatalogEventBus::Handler
        , protected GOATBackendRequestBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(GOATSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);


        ////////////////////////////////////////////////////////////////////////
        // IAgentSystem
        bool LoadScript(const AZ::Data::Asset<AZ::ScriptAsset>& asset) override;
        AZ::Outcome<void, AZStd::string> LoadBlackboard(const BlackboardAsset& asset) override;
        AZ::Outcome<void, AZStd::string> CompileProgram(
            const AZ::Name& backendName, const AZ::Name& programName) override;
        bool IsProgramCompiled(const AZ::Name& programName) const override;
        AgentId RegisterAgent(
            AZ::EntityId entity, const AZ::Name& backendName, AZStd::span<const AZ::Name> programs, size_t band,
            const AZ::Name& squad) override;
        void UnregisterAgent(AgentId agent) override;
        void WakeAgents(AZStd::span<const AgentId> agents) override;
        bool SetAgentTree(AgentId agent, const AZ::Name& treeName, AZ::u8 priority) override;
        bool PushAgentTree(AgentId agent, const AZ::Name& treeName, AZ::u8 priority) override;
        bool PopAgentTree(AgentId agent) override;
        AZ::Name GetAgentTree(AgentId agent) const override;
        void JoinSquad(AgentId agent, const AZ::Name& squad) override;
        void LeaveSquad(AgentId agent) override;
        AZ::Name GetAgentSquad(AgentId agent) const override;
        AZStd::vector<AgentId> GetAgents() const override;
        AgentId FindAgent(AZ::EntityId entity) const override;
        AZ::EntityId GetAgentEntity(AgentId agent) const override;
        bool SetAgentBand(AgentId agent, size_t band) override;
        size_t GetAgentBand(AgentId agent) const override;
        AZ::Outcome<size_t, AZStd::string> RebindSubtree(const AZ::Name& slot, const AZ::Name& treeName) override;
        bool RegisterDirector(AgentId director, const DirectorProfile& profile) override;
        void UnregisterDirector(AgentId director) override;
        size_t GetReachSize(AgentId director) override;
        AgentId GetInReach(AgentId director, size_t index) override;
        bool RegisterReachFilter(AZStd::unique_ptr<IReachFilter> filter) override;
        void UnregisterReachFilter(const AZ::Name& name) override;
        AZStd::vector<AZ::Name> GetReachFilterNames() const override;
        bool RegisterBackend(AZStd::unique_ptr<IBackend> backend) override;
        void UnregisterBackend(const AZ::Name& name) override;
        bool RegisterNodeType(NodeTypeDescriptor descriptor) override;
        void UnregisterNodeType(const AZ::Name& name) override;
        void RegisterVocabularyScript(AZStd::string_view assetPath) override;
        void UnregisterVocabularyScript(AZStd::string_view assetPath) override;
        ActionStateId RegisterAction(AZStd::unique_ptr<IActionState> action) override;
        void UnregisterAction(ActionStateId id) override;
        AZStd::vector<AZ::Name> GetBackendNames() const override;
        AZStd::vector<AZ::Name> GetActionNames() const override;
        AZStd::vector<AZ::Name> GetTreeNames() const override;
        AZStd::vector<AZ::Name> GetNodeTypeNames() const override;
        AZStd::string DescribeAgent(AgentId agent) const override;
        ////////////////////////////////////////////////////////////////////////

    protected:
        ////////////////////////////////////////////////////////////////////////
        // AzFramework::AssetCatalogEventBus
        //! System components activate before the asset catalog loads, so the vocabulary is
        //! picked up here rather than at activation.
        void OnCatalogLoaded(const char* catalogFile) override;
        ////////////////////////////////////////////////////////////////////////

        //! What a backend may reach of the core.
        AZ::Outcome<AZStd::shared_ptr<const AuthoredNode>, AZStd::string> EmitProgram(
            const AZ::Name& name) override;
        AZ::Name GetSubtreeBinding(const AZ::Name& slot) const override;
        ActionStateId FindVerb(const AZ::Name& name) const override;
        const NodeTypeDescriptor* FindNodeType(const AZ::Name& name) const override;
        IBackend* FindBackend(const AZ::Name& name) const override;
        ActionResult CallBehavior(
            const AZ::Name& behavior, const char* phase, AgentId agent, float deltaTime) override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // GOATBackendRequestBus
        bool RegisterDecisionBackend(AZStd::unique_ptr<IDecisionBackend>& backend) override;
        void UnregisterDecisionBackend(const AZ::Name& name) override;
        IDecisionBackend* FindDecisionBackend(const AZ::Name& name) const override;
        AZStd::vector<AZ::Name> GetDecisionBackendNames() const override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component
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
        void ListPlans(const AZ::ConsoleCommandContainer& arguments);
        void DumpPlan(const AZ::ConsoleCommandContainer& arguments);
        void ValidatePlans(const AZ::ConsoleCommandContainer& arguments);
        void SetAgentTreeCommand(const AZ::ConsoleCommandContainer& arguments);
        void SetAgentBandCommand(const AZ::ConsoleCommandContainer& arguments);
        void ListDirectors(const AZ::ConsoleCommandContainer& arguments);
        void DumpDirector(const AZ::ConsoleCommandContainer& arguments);
        void ListReachFilters(const AZ::ConsoleCommandContainer& arguments);
        void ListSquads(const AZ::ConsoleCommandContainer& arguments);
        void RebindSubtreeCommand(const AZ::ConsoleCommandContainer& arguments);
        void SetVariable(const AZ::ConsoleCommandContainer& arguments);
        void DumpVariable(const AZ::ConsoleCommandContainer& arguments);

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
        AZ_CONSOLEFUNC(GOATSystemComponent, ListPlans, AZ::ConsoleFunctorFlags::Null,
            "Lists the declarative plans the bt backend can satisfy");
        AZ_CONSOLEFUNC(GOATSystemComponent, DumpPlan, AZ::ConsoleFunctorFlags::Null,
            "Prints one plan's options, their guards and their steps, by name");
        AZ_CONSOLEFUNC(GOATSystemComponent, ValidatePlans, AZ::ConsoleFunctorFlags::Null,
            "Re-checks every declared plan against the registries and reports what is wrong");
        AZ_CONSOLEFUNC(GOATSystemComponent, SetAgentTreeCommand, AZ::ConsoleFunctorFlags::Null,
            "Puts one agent onto another of its trees, by entity id and tree name");
        AZ_CONSOLEFUNC(GOATSystemComponent, SetAgentBandCommand, AZ::ConsoleFunctorFlags::Null,
            "Moves one agent to another pacing band, by entity id and band index");
        AZ_CONSOLEFUNC(GOATSystemComponent, ListDirectors, AZ::ConsoleFunctorFlags::Null,
            "Lists every director, its reach and how many agents it governs");
        AZ_CONSOLEFUNC(GOATSystemComponent, DumpDirector, AZ::ConsoleFunctorFlags::Null,
            "Lists exactly the agents one director governs, by entity id");
        AZ_CONSOLEFUNC(GOATSystemComponent, ListReachFilters, AZ::ConsoleFunctorFlags::Null,
            "Lists the reach filters modules have contributed");
        AZ_CONSOLEFUNC(GOATSystemComponent, ListSquads, AZ::ConsoleFunctorFlags::Null,
            "Lists every squad that currently has a member");
        AZ_CONSOLEFUNC(GOATSystemComponent, RebindSubtreeCommand, AZ::ConsoleFunctorFlags::Null,
            "Points a subtree slot at another tree, by slot name and tree name");
        AZ_CONSOLEFUNC(GOATSystemComponent, SetVariable, AZ::ConsoleFunctorFlags::Null,
            "Writes a blackboard variable: <name> <value> [entityId]. The entity is needed only "
            "for an agent or squad scoped one, and names whose storage to write through");
        AZ_CONSOLEFUNC(GOATSystemComponent, DumpVariable, AZ::ConsoleFunctorFlags::Null,
            "Reads a blackboard variable back: <name> [entityId]");
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

        //! Runs a script from its cache path, preferring the compiled form.
        bool RunScript(AZStd::string_view assetPath, const char* what);

        //! Asks for a tree change, deferred to the agent's next tick because a request can
        //! arrive from Lua running inside that agent's current one.
        bool RequestTreeSwitch(AgentId agent, const AZ::Name& treeName, TreeSwitchKind kind, AZ::u8 priority);

        //! The archetype for a list of trees, building it on first use. Entities that declare
        //! the same trees share one, which is what makes a tree list cost per kind of agent
        //! rather than per agent.
        AZStd::shared_ptr<const AgentArchetype> AcquireArchetype(AZStd::span<const AZ::Name> trees);

        //! Every archetype in use, kept alive for the agents that share it.
        AZStd::vector<AZStd::shared_ptr<AgentArchetype>> m_archetypes;

        //! Refusals already reported, as the agent's slot paired with the tree it was refused.
        //! Diagnostics only: nothing reads it back, and an agent's entries go when it unregisters
        //! so a reused slot does not inherit another agent's silence.
        AZStd::unordered_set<AZ::u64> m_reportedRefusals;

        //! Carries out a deferred request. Installed on the runtime, which calls it first thing.
        void ApplyTreeSwitch(AgentRecord& agent);

        //! Runs the first of a list of alternative asset paths that loads.
        bool RunFirstAvailable(const char* const* paths, size_t count, const char* what);

        //! Installs the director vocabulary: the five verbs and the words that run them.
        void InstallDirectorVocabulary();

        //! Checks every declared plan and reports what is wrong with it.
        void ValidateLuaPlans();

        //! Gives every registered node type a word in the authoring vocabulary.
        void DeclareNodeWords();

        //! Gives one node type a word, filling its first required property from a bare string.
        void DeclareNodeWord(const NodeTypeDescriptor& descriptor);

        //! Loads the vocabulary if it is not loaded yet.
        //! Activation can run before the asset catalog is ready, so this retries on first use
        //! rather than leaving the gem permanently unable to compile a tree.
        bool EnsureVocabulary();

        //! Installs a backend front for every backend a script declared in Lua.
        void RegisterLuaBackends();

        //! Where every plan's steps live. Outlives the agents, because their plans are spans
        //! into it rather than copies.
        AZStd::unique_ptr<PlanStore> m_planStore;
        AZStd::unique_ptr<ReachFilterRegistry> m_reachFilters;
        AZStd::unique_ptr<DirectorRegistry> m_directors;
        DirectorKeys m_directorKeys;
        AZStd::unique_ptr<BlackboardSystem> m_blackboardSystem;
        AZStd::unique_ptr<ActionStateRegistry> m_actions;
        AZStd::unique_ptr<BackendRegistry> m_backends;
        AZStd::unique_ptr<DecisionBackendRegistry> m_decisionBackends;
        AZStd::unique_ptr<NodeTypeRegistry> m_nodeTypes;
        AZStd::unique_ptr<TreeLibrary> m_trees;
        AZStd::unique_ptr<LuaDispatch> m_dispatch;
        //! Stable for the gem's lifetime, because Lua receives a raw pointer to it.
        AZStd::unique_ptr<AgentScriptContext> m_scriptContext;
        AZStd::unique_ptr<LuaNodeScripting> m_scripting;
        AZStd::unique_ptr<AgentRuntime> m_runtime;
        AZStd::unique_ptr<AgentRegistry> m_agents;

        //! Trees compiled so far, shared by every agent running the same one.
        AZStd::unordered_map<AZ::Name, AZStd::shared_ptr<const AgentProgram>> m_programs;
        AZStd::vector<AZStd::unique_ptr<AZ::Data::AssetHandler>> m_assetHandlers;
        //! Vocabulary files backend gems ship, run in the order they registered.
        AZStd::vector<AZStd::string> m_vocabularyScripts;
        //! Whether the authoring vocabulary is loaded into the script context.
        bool m_vocabularyLoaded = false;
    };
} // namespace GOAT
