#include "GOATSystemComponent.h"

#include <Core/Assets/BlackboardAssetHandler.h>
#include <Core/Actions/RunScriptAction.h>
#include <Core/Actions/WaitAction.h>
#include <Core/Frontend/DirectBackend.h>
#include <Core/Frontend/TreeCompiler.h>
#include <Core/Scripting/LuaBackend.h>
#include <Core/Scripting/LuaNameCollector.h>
#include <Core/Scripting/LuaPlanBuilder.h>
#include <Core/Scripting/LuaPlanValidator.h>
#include <Core/Scripting/LuaTreeBuilder.h>

#include <GOAT/Assets/BehaviorTreeAsset.h>
#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/Domain/ActionPlan.h>
#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/Domain/BlackboardTypes.h>
#include <GOAT/Domain/Guard.h>
#include <GOAT/Domain/Intent.h>
#include <GOAT/Domain/NodeType.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Console/ConsoleTypeHelpers.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Asset/GenericAssetHandler.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATSystemComponent, "GOATSystemComponent", GOATSystemComponentTypeId);

    namespace
    {
        //! Where the authoring vocabulary lands in the cache.
        //! A scan folder contributes no prefix of its own, so the gem's assets live under an
        //! Assets/GOAT/ subfolder to keep this path from colliding with a project's own
        //! Scripts/GOAT.lua. LuaBuilder emits .luac, so prefer the compiled form.
        constexpr const char* VocabularyAssetPaths[] = {
            "goat/scripts/goat.luac",
            "goat/scripts/goat.lua",
        };

        //! Backends the gem ships, loaded straight after the vocabulary and before any user
        //! script, so a tree may delegate to one without the project declaring it.
        constexpr const char* BackendAssetPaths[] = {
            "goat/backends/behaviortreebackend.luac",
            "goat/backends/behaviortreebackend.lua",
        };
    } // namespace

    void GOATSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectBlackboardTypes(context);
        ReflectActionTypes(context);
        ReflectGuardTypes(context);
        ReflectNodeTypes(context);
        BlackboardKey::Reflect(context);
        Intent::Reflect(context);
        BlackboardAsset::Reflect(context);
        BehaviorTreeAsset::Reflect(context);
        LuaTreeBuilder::Reflect(context);
        LuaPlanBuilder::Reflect(context);
        LuaPlanValidator::Reflect(context);
        LuaNameCollector::Reflect(context);
        AgentScriptContext::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOATSystemComponent, AZ::Component>()->Version(0);
        }
    }

    void GOATSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOATService"));
        // Signals that this component registers generic asset handlers, so builders wait for it.
        provided.push_back(AzFramework::s_GenericAssetRegistrar);
    }

    void GOATSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOATService"));
    }

    void GOATSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void GOATSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC_CE("ScriptService"));
        dependent.push_back(AZ_CRC_CE("AssetDatabaseService"));
    }

    GOATSystemComponent::GOATSystemComponent()
    {
        if (GOATInterface::Get() == nullptr)
        {
            GOATInterface::Register(this);
        }
    }

    GOATSystemComponent::~GOATSystemComponent()
    {
        if (GOATInterface::Get() == this)
        {
            GOATInterface::Unregister(this);
        }
    }

    void GOATSystemComponent::Init()
    {
    }

    void GOATSystemComponent::Activate()
    {
        StartServices();
        RegisterAssetHandlers();
        GOATRequestBus::Handler::BusConnect();
        AzFramework::AssetCatalogEventBus::Handler::BusConnect();

        if (AgentSystemInterface::Get() == nullptr)
        {
            AgentSystemInterface::Register(this);
        }
    }

    void GOATSystemComponent::Deactivate()
    {
        if (AgentSystemInterface::Get() == this)
        {
            AgentSystemInterface::Unregister(this);
        }

        AzFramework::AssetCatalogEventBus::Handler::BusDisconnect();
        GOATRequestBus::Handler::BusDisconnect();
        UnregisterAssetHandlers();
        StopServices();
    }

    void GOATSystemComponent::StartServices()
    {
        m_planStore = AZStd::make_unique<PlanStore>();
        m_blackboardSystem = AZStd::make_unique<BlackboardSystem>();
        m_actions = AZStd::make_unique<ActionStateRegistry>();
        m_backends = AZStd::make_unique<BackendRegistry>();
        m_nodeTypes = AZStd::make_unique<NodeTypeRegistry>();
        m_trees = AZStd::make_unique<TreeLibrary>();
        m_dispatch = AZStd::make_unique<LuaDispatch>();
        m_scriptContext = AZStd::make_unique<AgentScriptContext>();

        // The direct backend is frontend plumbing, not a backend algorithm: it is what lets a
        // plainly authored leaf reach the state machine by the same route as a plan. It is
        // registered like any other backend so a leaf naming it by name resolves normally.
        auto direct = AZStd::make_unique<DirectBackend>();
        m_directBackend = direct.get();
        m_backends->Register(AZStd::move(direct));

        m_actions->RegisterAt(CoreActions::Wait, AZStd::make_unique<WaitAction>());
        m_actions->RegisterAt(CoreActions::RunScript, AZStd::make_unique<RunScriptAction>(*m_dispatch, *m_scriptContext));

        m_scripting = AZStd::make_unique<LuaNodeScripting>(*m_dispatch, *m_scriptContext);
        m_runtime = AZStd::make_unique<AgentRuntime>(
            *m_blackboardSystem, *m_actions, *m_backends, *m_directBackend, *m_dispatch, *m_scriptContext,
            *m_scripting, *m_planStore);
        m_agents = AZStd::make_unique<AgentRegistry>(*m_runtime, *m_blackboardSystem, *m_dispatch);

        m_dispatch->ConfigurePlanBuilder(m_actions.get(), m_blackboardSystem.get(), m_planStore.get());
        m_vocabularyLoaded = false;
        m_dispatch->Connect();
    }

    void GOATSystemComponent::StopServices()
    {
        m_programs.clear();
        m_agents.reset();
        m_runtime.reset();
        m_directBackend = nullptr;
        m_scripting.reset();
        m_scriptContext.reset();
        if (m_dispatch != nullptr)
        {
            m_dispatch->Disconnect();
        }
        m_dispatch.reset();
        m_trees.reset();
        m_nodeTypes.reset();
        m_backends.reset();
        m_actions.reset();
        m_blackboardSystem.reset();

        // Last, because every agent's plan was a span into it.
        m_planStore.reset();
    }

    void GOATSystemComponent::DeclareNodeWord(const NodeTypeDescriptor& descriptor)
    {
        AZ_Assert(m_dispatch != nullptr, "Declaring a node word needs the Lua dispatch");
        if (m_dispatch == nullptr)
        {
            return;
        }

        // A bare string argument fills the first required property, which is the rule the
        // built-in words already follow: `wait "2"` sets seconds, `move_to "goal"` sets key.
        AZ::Name mainProperty;
        for (const NodeParameter& parameter : descriptor.m_parameters)
        {
            if (parameter.m_required)
            {
                mainProperty = parameter.m_name;
                break;
            }
        }

        m_dispatch->DeclareNode(descriptor.m_name, mainProperty);
    }

    void GOATSystemComponent::DeclareNodeWords()
    {
        AZ_Assert(m_nodeTypes != nullptr, "Declaring node words needs the node type registry");
        if (m_nodeTypes == nullptr)
        {
            return;
        }

        for (const NodeTypeDescriptor* descriptor : m_nodeTypes->GetAll())
        {
            AZ_Assert(descriptor != nullptr, "The node type registry must not hold a null descriptor");
            if (descriptor != nullptr)
            {
                DeclareNodeWord(*descriptor);
            }
        }
    }

    bool GOATSystemComponent::RunFirstAvailable(const char* const* paths, size_t count, const char* what)
    {
        AZ_Assert(paths != nullptr && count > 0, "Running a script needs somewhere to look for it");

        // The paths are alternatives, best first: LuaBuilder emits .luac, so the compiled form is
        // preferred and the plain one is the fallback for a project that ships source.
        for (size_t i = 0; i < count; ++i)
        {
            const char* path = paths[i];

            AZ::Data::AssetId assetId;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                assetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath, path, azrtti_typeid<AZ::ScriptAsset>(), false);

            if (!assetId.IsValid())
            {
                AZLOG_INFO("GOAT: no asset registered at '%s'", path);
                continue;
            }

            AZLOG_INFO("GOAT: loading %s from '%s'", what, path);

            auto asset = AZ::Data::AssetManager::Instance().GetAsset<AZ::ScriptAsset>(
                assetId, AZ::Data::AssetLoadBehavior::PreLoad);
            asset.BlockUntilLoadComplete();

            if (!asset.IsReady())
            {
                AZLOG_INFO("GOAT: '%s' is registered but did not load", path);
                continue;
            }

            if (m_dispatch->RunScript(asset))
            {
                return true;
            }

            AZLOG_INFO("GOAT: '%s' loaded but failed to run", path);
        }

        return false;
    }

    bool GOATSystemComponent::LoadVocabulary()
    {
        if (!RunFirstAvailable(VocabularyAssetPaths, AZStd::size(VocabularyAssetPaths), "the authoring vocabulary"))
        {
            AZ_Warning(
                "GOAT", false,
                "Could not load the GOAT Lua vocabulary; check that the Asset Processor produced goat/scripts/goat.luac");
            return false;
        }

        // The backends the gem ships load straight after the words they are written in, and
        // before any user script, so a tree may delegate to one without declaring it.
        AZ_Warning("GOAT", RunFirstAvailable(BackendAssetPaths, AZStd::size(BackendAssetPaths), "the shipped backends"),
            "Could not load GOAT's shipped backends; delegate \"bt\" will not resolve");

        return true;
    }

    bool GOATSystemComponent::EnsureVocabulary()
    {
        if (m_vocabularyLoaded)
        {
            return true;
        }

        if (m_dispatch == nullptr || (!m_dispatch->IsReady() && !m_dispatch->Connect()))
        {
            return false;
        }

        m_vocabularyLoaded = LoadVocabulary();
        if (m_vocabularyLoaded)
        {
            // Covers node types registered by a module before the vocabulary was available.
            DeclareNodeWords();

            // The shipped backends were declared by the file just run, so install them now
            // rather than waiting for the first user script to trigger a sweep.
            RegisterLuaBackends();
        }
        return m_vocabularyLoaded;
    }

    bool GOATSystemComponent::LoadScript(const AZ::Data::Asset<AZ::ScriptAsset>& asset)
    {
        // A user script calls tree() and behavior(), which the vocabulary defines.
        if (!EnsureVocabulary())
        {
            AZ_Warning("GOAT", false, "Cannot run a GOAT script before the authoring vocabulary is loaded");
            return false;
        }

        if (m_dispatch == nullptr || !m_dispatch->RunScript(asset))
        {
            return false;
        }

        RegisterLuaBackends();

        // Baking turns every authored plan into steps C++ already holds, so running one later
        // pushes nothing across this boundary. Validation then reports what is wrong with them
        // while the author is still looking at the file.
        m_dispatch->BakePlans();
        ValidateLuaPlans();

        // A bad plan does not fail the load: one malformed plan must not stop every tree in the
        // same file from compiling. It fails later through the refusal path the walker handles.
        return true;
    }

    void GOATSystemComponent::ValidateLuaPlans()
    {
        AZ_Assert(m_dispatch != nullptr, "Checking plans needs the Lua dispatch");
        if (m_dispatch == nullptr)
        {
            return;
        }

        m_dispatch->ValidatePlans();

        const LuaPlanValidator& validator = m_dispatch->GetPlanValidator();
        for (const AZStd::string& problem : validator.GetProblems())
        {
            AZ_Error("GOAT", false, "%s", problem.c_str());
        }

        if (validator.IsClean())
        {
            AZLOG(GoatPlan, "GOAT: %zu plan(s) checked with no problems", validator.GetSummaries().size());
        }
    }

    void GOATSystemComponent::RegisterLuaBackends()
    {
        // A script may declare backends, so install one front per name that is not already
        // taken. A C++ backend registered under the same name wins, since it registered first.
        for (const AZ::Name& name : m_dispatch->GetLuaBackendNames())
        {
            if (m_backends->Find(name) != nullptr)
            {
                continue;
            }
            m_backends->Register(AZStd::make_unique<LuaBackend>(name, *m_dispatch, *m_scriptContext));
        }
    }

    AZ::Outcome<void, AZStd::string> GOATSystemComponent::LoadBlackboard(const BlackboardAsset& asset)
    {
        if (m_blackboardSystem == nullptr)
        {
            return AZ::Failure(AZStd::string("The blackboard system is not running"));
        }

        for (const BlackboardVariable& variable : asset.m_variables)
        {
            auto declared = m_blackboardSystem->Declare(
                AZ::Name(variable.m_name), variable.m_scope, variable.m_type, variable.GetDefault());
            if (!declared.IsSuccess())
            {
                return AZ::Failure(declared.TakeError());
            }
        }
        return AZ::Success();
    }

    AZ::Outcome<void, AZStd::string> GOATSystemComponent::CompileTree(const AZ::Name& treeName)
    {
        if (m_dispatch == nullptr || m_trees == nullptr)
        {
            return AZ::Failure(AZStd::string("The scripting services are not running"));
        }

        if (!EnsureVocabulary())
        {
            return AZ::Failure(AZStd::string(
                "The GOAT authoring vocabulary is not loaded; check that goat/scripts/goat.lua reached the cache"));
        }

        // Ask Lua for the authored tree, then compile it exactly as a graph editor's asset would be.
        auto emitted = m_dispatch->EmitTree(treeName);
        if (!emitted.IsSuccess())
        {
            return AZ::Failure(emitted.TakeError());
        }

        m_trees->Add(treeName, emitted.GetValue());

        const TreeCompiler compiler(*m_nodeTypes, *m_blackboardSystem, *m_trees, *m_actions);
        auto compiled = compiler.Compile(treeName, *emitted.GetValue());
        if (!compiled.IsSuccess())
        {
            return AZ::Failure(compiled.TakeError());
        }

        m_programs[treeName] =
            AZStd::shared_ptr<const DecisionProgram>(aznew DecisionProgram(AZStd::move(compiled.GetValue())));

        AZ_Assert(IsTreeCompiled(treeName), "Compiling a tree must leave a program agents can be registered against");
        return AZ::Success();
    }

    bool GOATSystemComponent::IsTreeCompiled(const AZ::Name& treeName) const
    {
        AZ_Assert(!treeName.IsEmpty(), "A tree is always asked about by name");
        return m_programs.find(treeName) != m_programs.end();
    }

    AgentId GOATSystemComponent::RegisterAgent(AZ::EntityId entity, const AZ::Name& treeName, size_t band)
    {
        if (m_agents == nullptr)
        {
            return AgentId{};
        }

        const auto program = m_programs.find(treeName);
        if (program == m_programs.end())
        {
            AZ_Warning("GOAT", false, "Tree '%s' has not been compiled", treeName.GetCStr());
            return AgentId{};
        }

        return m_agents->Register(entity, program->second, band);
    }

    void GOATSystemComponent::UnregisterAgent(AgentId agent)
    {
        if (m_agents != nullptr)
        {
            m_agents->Unregister(agent);
        }
    }

    void GOATSystemComponent::JoinSquad(AgentId agent, const AZ::Name& squad)
    {
        if (m_blackboardSystem != nullptr)
        {
            m_blackboardSystem->JoinSquad(agent, squad);
        }
    }

    bool GOATSystemComponent::RegisterBackend(AZStd::unique_ptr<IBackend> backend)
    {
        return m_backends != nullptr && m_backends->Register(AZStd::move(backend));
    }

    void GOATSystemComponent::UnregisterBackend(const AZ::Name& name)
    {
        if (m_backends != nullptr)
        {
            m_backends->Unregister(name);
        }
    }

    bool GOATSystemComponent::RegisterNodeType(NodeTypeDescriptor descriptor)
    {
        AZ_Assert(!descriptor.m_name.IsEmpty(), "A node type must be registered under a name");
        if (m_nodeTypes == nullptr || descriptor.m_name.IsEmpty())
        {
            AZ_Error("GOAT", false, "Cannot register a node type before the node type registry exists");
            return false;
        }

        const AZ::Name name = descriptor.m_name;
        if (!m_nodeTypes->Register(AZStd::move(descriptor)))
        {
            AZ_Error("GOAT", false, "Node type '%s' is already registered", name.GetCStr());
            return false;
        }

        // A module may register before or after the vocabulary loads, so cover both:
        // this call handles the late case and DeclareNodeWords handles the early one.
        const NodeTypeDescriptor* registered = m_nodeTypes->Find(name);
        AZ_Assert(registered != nullptr, "A node type must be findable immediately after registering");
        if (registered != nullptr && m_vocabularyLoaded)
        {
            DeclareNodeWord(*registered);
        }

        return true;
    }

    void GOATSystemComponent::UnregisterNodeType(const AZ::Name& name)
    {
        if (m_nodeTypes != nullptr)
        {
            m_nodeTypes->Unregister(name);
        }
    }

    ActionStateId GOATSystemComponent::RegisterAction(AZStd::unique_ptr<IActionState> action)
    {
        // Narrow on purpose: `wait`, `move_to` and `claim_smart_object` are all legitimately both
        // a node word and a verb. Only `delegate` is reserved, because a plan step naming it
        // would be the one way a plan could re-enter the tree that asked for it.
        AZ_Assert(action == nullptr || action->GetName() != AZ_NAME_LITERAL("delegate"),
            "A verb may not be registered as 'delegate': that is the tree word for entering a "
            "backend, and a plan step naming it would let a plan re-enter the tree");

        return m_actions != nullptr ? m_actions->Register(AZStd::move(action)) : CoreActions::Invalid;
    }

    void GOATSystemComponent::UnregisterAction(ActionStateId id)
    {
        if (m_actions != nullptr)
        {
            m_actions->Unregister(id);
        }
    }

    AZStd::vector<AZ::Name> GOATSystemComponent::GetBackendNames() const
    {
        // The direct backend is in the registry like any other, so no special case here.
        return m_backends != nullptr ? m_backends->GetNames() : AZStd::vector<AZ::Name>{};
    }

    AZStd::vector<AZ::Name> GOATSystemComponent::GetActionNames() const
    {
        return m_actions != nullptr ? m_actions->GetNames() : AZStd::vector<AZ::Name>{};
    }

    AZStd::vector<AZ::Name> GOATSystemComponent::GetNodeTypeNames() const
    {
        AZStd::vector<AZ::Name> names;
        if (m_nodeTypes == nullptr)
        {
            return names;
        }

        for (const NodeTypeDescriptor* descriptor : m_nodeTypes->GetAll())
        {
            AZ_Assert(descriptor != nullptr, "The node type registry must not hold a null descriptor");
            if (descriptor != nullptr)
            {
                names.push_back(descriptor->m_name);
            }
        }
        return names;
    }

    AZStd::vector<AZ::Name> GOATSystemComponent::GetTreeNames() const
    {
        AZStd::vector<AZ::Name> names;
        names.reserve(m_programs.size());
        for (const auto& [name, program] : m_programs)
        {
            names.push_back(name);
        }
        return names;
    }

    AZStd::string GOATSystemComponent::DescribeAgent(AgentId agent) const
    {
        if (m_agents == nullptr)
        {
            return "the agent system is not running";
        }

        AgentRecord* record = const_cast<AgentRegistry*>(m_agents.get())->Find(agent);
        if (record == nullptr)
        {
            return "no such agent";
        }

        const ActionRequest* action = record->m_machine.GetCurrentAction();
        const AZ::Name verb = action != nullptr && m_actions->Find(action->m_action) != nullptr
            ? m_actions->Find(action->m_action)->GetName()
            : AZ::Name("idle");

        return AZStd::string::format(
            "tree '%s' band %zu node %u action '%s' step %zu elapsed %.2fs",
            record->m_program != nullptr ? record->m_program->m_name.GetCStr() : "<none>", record->m_band,
            record->m_cursor.GetActiveLeaf(), verb.GetCStr(), record->m_machine.GetStepIndex(),
            record->m_machine.GetElapsed());
    }

    void GOATSystemComponent::ListBackends([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
    {
        for (const AZ::Name& name : GetBackendNames())
        {
            AZLOG_INFO("backend: %s", name.GetCStr());
        }
    }

    void GOATSystemComponent::ListActions([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
    {
        for (const AZ::Name& name : GetActionNames())
        {
            AZLOG_INFO("action: %s", name.GetCStr());
        }
    }

    void GOATSystemComponent::ListNodes([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
    {
        if (m_nodeTypes == nullptr)
        {
            return;
        }

        for (const NodeTypeDescriptor* descriptor : m_nodeTypes->GetAll())
        {
            AZLOG_INFO(
                "node: %-18s %-10s %s", descriptor->m_name.GetCStr(), descriptor->m_category.c_str(),
                descriptor->m_description.c_str());
        }
    }

    void GOATSystemComponent::ListTrees([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
    {
        for (const AZ::Name& name : GetTreeNames())
        {
            const auto program = m_programs.find(name);
            AZLOG_INFO(
                "tree: %s (%zu nodes, %zu guards, %zu services)", name.GetCStr(), program->second->m_nodes.size(),
                program->second->m_guardNodes.size(), program->second->m_services.size());
        }
    }

    void GOATSystemComponent::ListAgents([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
    {
        if (m_agents == nullptr)
        {
            return;
        }

        for (const AgentId agent : m_agents->GetAgents())
        {
            AgentRecord* record = m_agents->Find(agent);
            AZLOG_INFO(
                "agent %u: entity %s | %s", agent.GetIndex(),
                record != nullptr ? record->m_entity.ToString().c_str() : "<none>", DescribeAgent(agent).c_str());
        }
    }

    void GOATSystemComponent::ListPlans(const AZ::ConsoleCommandContainer&)
    {
        if (m_dispatch == nullptr)
        {
            return;
        }

        const auto& summaries = m_dispatch->GetPlanValidator().GetSummaries();
        if (summaries.empty())
        {
            // Nothing has been checked yet, so check now rather than reporting an empty list
            // that only means "no one has asked".
            ValidateLuaPlans();
        }

        for (const auto& summary : m_dispatch->GetPlanValidator().GetSummaries())
        {
            AZLOG_INFO("plan: %-24s %zu option(s)   %s", summary.m_name.GetCStr(),
                summary.m_options.size(), summary.m_source.c_str());
        }
    }

    void GOATSystemComponent::DumpPlan(const AZ::ConsoleCommandContainer& arguments)
    {
        if (arguments.empty() || m_dispatch == nullptr)
        {
            AZLOG_INFO("usage: GOATSystemComponent.DumpPlan <name>");
            return;
        }

        const AZ::Name wanted(AZStd::string(arguments.front()));
        const auto* summary = m_dispatch->GetPlanValidator().FindSummary(wanted);
        if (summary == nullptr)
        {
            AZLOG_INFO("no plan named '%s' has been declared", wanted.GetCStr());
            return;
        }

        AZLOG_INFO("plan '%s' declared in %s", summary->m_name.GetCStr(), summary->m_source.c_str());
        for (size_t option = 0; option < summary->m_options.size(); ++option)
        {
            const auto& entry = summary->m_options[option];
            if (entry.m_guard.empty())
            {
                AZLOG_INFO("  option %zu  (fallback)", option + 1);
            }
            else
            {
                AZLOG_INFO("  option %zu  %s %s", option + 1, entry.m_negated ? "unless" : "when",
                    entry.m_guard.c_str());
            }

            for (size_t step = 0; step < entry.m_lines.size(); ++step)
            {
                AZLOG_INFO("    %zu  %s", step + 1, entry.m_lines[step].c_str());
            }
        }
    }

    void GOATSystemComponent::ValidatePlans(const AZ::ConsoleCommandContainer&)
    {
        ValidateLuaPlans();

        if (m_dispatch != nullptr && m_dispatch->GetPlanValidator().IsClean())
        {
            AZLOG_INFO("%zu plan(s), no problems", m_dispatch->GetPlanValidator().GetSummaries().size());
        }
    }

    void GOATSystemComponent::DumpAgent(const AZ::ConsoleCommandContainer& arguments)
    {
        if (arguments.empty() || m_agents == nullptr)
        {
            AZLOG_INFO("usage: GOATSystemComponent.DumpAgent <entityId>");
            return;
        }

        AZ::u64 rawEntityId = 0;
        if (!AZ::ConsoleTypeHelpers::ToValue(rawEntityId, arguments.front()))
        {
            AZLOG_INFO("could not read '%.*s' as an entity id",
                aznumeric_cast<int>(arguments.front().size()), arguments.front().data());
            return;
        }

        const AZ::EntityId wanted(rawEntityId);
        for (const AgentId agent : m_agents->GetAgents())
        {
            const AgentRecord* record = m_agents->Find(agent);
            if (record != nullptr && record->m_entity == wanted)
            {
                AZLOG_INFO("agent %u: %s", agent.GetIndex(), DescribeAgent(agent).c_str());
                return;
            }
        }

        AZLOG_INFO("no agent is running on entity %s", wanted.ToString().c_str());
    }

    void GOATSystemComponent::OnCatalogLoaded([[maybe_unused]] const char* catalogFile)
    {
        // The catalog is up now, so the vocabulary can finally be found.
        EnsureVocabulary();
    }

    void GOATSystemComponent::ReloadVocabulary([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
    {
        // Forces a fresh attempt, so a failure at activation can be diagnosed once the
        // asset catalog is up rather than only being reported as a warning at startup.
        m_vocabularyLoaded = false;
        if (EnsureVocabulary())
        {
            AZLOG_INFO("GOAT: vocabulary loaded; %zu node types available", m_nodeTypes->GetAll().size());
        }
        else
        {
            AZLOG_INFO("GOAT: vocabulary still unavailable");
        }
    }

    void GOATSystemComponent::RegisterAssetHandlers()
    {
        // The launcher loads one module and the editor another, so only the first registration wins.
        if (AZ::Data::AssetManager::Instance().GetHandler(azrtti_typeid<BlackboardAsset>()) != nullptr)
        {
            return;
        }

        auto handler = AZStd::make_unique<BlackboardAssetHandler>();
        handler->Register();
        m_assetHandlers.emplace_back(AZStd::move(handler));
    }

    void GOATSystemComponent::UnregisterAssetHandlers()
    {
        if (AZ::Data::AssetManager::IsReady())
        {
            for (auto& handler : m_assetHandlers)
            {
                AZ::Data::AssetManager::Instance().UnregisterHandler(handler.get());
            }
        }
        m_assetHandlers.clear();
    }
} // namespace GOAT
