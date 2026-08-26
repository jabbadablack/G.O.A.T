#include "GOATSystemComponent.h"

#include <Core/Actions/RunScriptAction.h>
#include <Core/Actions/WaitAction.h>
#include <Core/Frontend/DirectBackend.h>
#include <Core/Frontend/TreeCompiler.h>
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
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Asset/GenericAssetHandler.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATSystemComponent, "GOATSystemComponent", GOATSystemComponentTypeId);

    namespace
    {
        //! Where the authoring vocabulary ships, relative to the gem's asset scan folder.
        constexpr const char* VocabularyAssetPaths[] = { "scripts/goat.luac", "scripts/goat.lua" };
    } // namespace

    void GOATSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectBlackboardTypes(context);
        ReflectActionTypes(context);
        ReflectGuardTypes(context);
        ReflectNodeTypes(context);
        BlackboardKey::Reflect(context);
        Intent::Reflect(context);
        ActionPlan::Reflect(context);
        BlackboardAsset::Reflect(context);
        BehaviorTreeAsset::Reflect(context);
        LuaTreeBuilder::Reflect(context);
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

        GOATRequestBus::Handler::BusDisconnect();
        UnregisterAssetHandlers();
        StopServices();
    }

    void GOATSystemComponent::StartServices()
    {
        m_blackboardSystem = AZStd::make_unique<BlackboardSystem>();
        m_actions = AZStd::make_unique<ActionStateRegistry>();
        m_backends = AZStd::make_unique<BackendRegistry>();
        m_nodeTypes = AZStd::make_unique<NodeTypeRegistry>();
        m_trees = AZStd::make_unique<TreeLibrary>();
        m_dispatch = AZStd::make_unique<LuaDispatch>();
        m_scriptContext = AZStd::make_unique<AgentScriptContext>();

        // The direct backend is frontend plumbing, not a backend algorithm: it is what
        // lets a plainly authored leaf reach the state machine by the same route as a plan.
        auto direct = AZStd::make_unique<DirectBackend>();
        m_directBackend = AZStd::move(direct);

        m_actions->RegisterAt(CoreActions::Wait, AZStd::make_unique<WaitAction>());
        m_actions->RegisterAt(CoreActions::RunScript, AZStd::make_unique<RunScriptAction>(*m_dispatch, *m_scriptContext));

        m_runtime = AZStd::make_unique<AgentRuntime>(
            *m_blackboardSystem, *m_actions, *m_backends, *m_directBackend, *m_dispatch, *m_scriptContext);
        m_agents = AZStd::make_unique<AgentRegistry>(*m_runtime, *m_blackboardSystem, *m_dispatch);

        if (m_dispatch->Connect())
        {
            LoadVocabulary();
        }
    }

    void GOATSystemComponent::StopServices()
    {
        m_programs.clear();
        m_agents.reset();
        m_runtime.reset();
        m_directBackend.reset();
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
    }

    bool GOATSystemComponent::LoadVocabulary()
    {
        for (const char* path : VocabularyAssetPaths)
        {
            AZ::Data::AssetId assetId;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                assetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath, path, azrtti_typeid<AZ::ScriptAsset>(), false);

            if (!assetId.IsValid())
            {
                continue;
            }

            auto asset = AZ::Data::AssetManager::Instance().GetAsset<AZ::ScriptAsset>(
                assetId, AZ::Data::AssetLoadBehavior::PreLoad);
            asset.BlockUntilLoadComplete();

            if (asset.IsReady() && m_dispatch->RunScript(asset))
            {
                return true;
            }
        }

        AZ_Warning(
            "GOAT", false,
            "Could not load the GOAT Lua vocabulary; trees cannot be authored until scripts/goat.lua is processed");
        return false;
    }

    bool GOATSystemComponent::LoadScript(const AZ::Data::Asset<AZ::ScriptAsset>& asset)
    {
        return m_dispatch != nullptr && m_dispatch->RunScript(asset);
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

        // Ask Lua for the authored tree, then compile it exactly as a graph editor's asset would be.
        auto emitted = m_dispatch->EmitTree(treeName);
        if (!emitted.IsSuccess())
        {
            return AZ::Failure(emitted.TakeError());
        }

        m_trees->Add(treeName, emitted.GetValue());

        const TreeCompiler compiler(*m_nodeTypes, *m_blackboardSystem, *m_trees);
        auto compiled = compiler.Compile(treeName, *emitted.GetValue());
        if (!compiled.IsSuccess())
        {
            return AZ::Failure(compiled.TakeError());
        }

        m_programs[treeName] =
            AZStd::shared_ptr<const DecisionProgram>(aznew DecisionProgram(AZStd::move(compiled.GetValue())));
        return AZ::Success();
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

    ActionStateId GOATSystemComponent::RegisterAction(AZStd::unique_ptr<IActionState> action)
    {
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
        AZStd::vector<AZ::Name> names;
        if (m_backends != nullptr)
        {
            names = m_backends->GetNames();
        }
        // The direct backend is always present, whatever else is installed.
        names.push_back(DirectBackend::GetBackendName());
        return names;
    }

    AZStd::vector<AZ::Name> GOATSystemComponent::GetActionNames() const
    {
        return m_actions != nullptr ? m_actions->GetNames() : AZStd::vector<AZ::Name>{};
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

    void GOATSystemComponent::RegisterAssetHandlers()
    {
        // The launcher loads one module and the editor another, so only the first registration wins.
        if (AZ::Data::AssetManager::Instance().GetHandler(azrtti_typeid<BlackboardAsset>()) != nullptr)
        {
            return;
        }

        auto handler = AZStd::make_unique<AzFramework::GenericAssetHandler<BlackboardAsset>>(
            BlackboardAsset::DisplayName, BlackboardAsset::AssetGroup, BlackboardAsset::FileExtension);
        // Lets the Asset Processor build the source into the cache without a custom builder.
        handler->SetAutoBuildAssetToCache(true);
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
