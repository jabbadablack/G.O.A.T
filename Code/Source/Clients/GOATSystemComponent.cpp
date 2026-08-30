#include "GOATSystemComponent.h"

#include <GOAT/GOATRemoteDebug.h>

#include <Core/Application/DecisionBackendAdapter.h>
#include <Core/Application/NestedRun.h>
#include <Core/Assets/BlackboardAssetHandler.h>
#include <Core/Assets/ProgramAssetHandler.h>
#include <Core/Actions/EmbedAction.h>
#include <Core/Actions/RunScriptAction.h>
#include <Core/Actions/WaitAction.h>
#include <Core/Director/DirectorActions.h>
#include <Core/Scripting/LuaBackend.h>
#include <Core/Scripting/LuaNameCollector.h>
#include <Core/Scripting/LuaPlanBuilder.h>
#include <Core/Scripting/LuaPlanValidator.h>
#include <Core/Scripting/LuaTreeBuilder.h>

#include <GOAT/Assets/ProgramAsset.h>
#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/Domain/ActionPlan.h>
#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/Domain/BlackboardTypes.h>
#include <GOAT/Domain/Intent.h>
#include <GOAT/Domain/NodeType.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Console/ConsoleTypeHelpers.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>
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
        ReflectNodeTypes(context);
        BlackboardKey::Reflect(context);
        Intent::Reflect(context);
        BlackboardAsset::Reflect(context);
        ProgramAsset::Reflect(context);
        AgentSnapshot::Reflect(context);
        ReflectRemoteDebug(context);
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

    void GOATSystemComponent::Activate()
    {
        StartServices();
        RegisterAssetHandlers();
        AzFramework::AssetCatalogEventBus::Handler::BusConnect();
        GOATBackendRequestBus::Handler::BusConnect();

        if (AgentSystemInterface::Get() == nullptr)
        {
            AgentSystemInterface::Register(this);
        }

        StartRemoteDebug();
    }

    void GOATSystemComponent::Deactivate()
    {
        StopRemoteDebug();

        if (AgentSystemInterface::Get() == this)
        {
            AgentSystemInterface::Unregister(this);
        }

        GOATBackendRequestBus::Handler::BusDisconnect();
        AzFramework::AssetCatalogEventBus::Handler::BusDisconnect();
        UnregisterAssetHandlers();
        StopServices();
    }

    void GOATSystemComponent::StartRemoteDebug()
    {
#if defined(ENABLE_REMOTE_TOOLS)
        // The tool listens and the game dials in, so that a game can start and stop as often as
        // it likes while the tool stays open. The editor registers the other half of this pair,
        // and a process must never be both for one key -- they write the same registry entry.
        AZ::ApplicationTypeQuery appType;
        AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationBus::Events::QueryApplicationType, appType);
        if (appType.IsEditor())
        {
            // The editor registers the listening half of this service instead. A process must
            // never register both halves of one key: they write the same registry entry.
            return;
        }

        auto* remoteTools = AzFramework::RemoteToolsInterface::Get();
        if (remoteTools == nullptr)
        {
            // The RemoteTools gem is not enabled, or this is a release build. Neither is a
            // fault: it only means no tool can attach to this process.
            AZLOG_INFO("GOAT: no remote tools service, so no tool can attach to this process");
            return;
        }

        remoteTools->RegisterToolingServiceClient(GoatToolsKey, GoatToolsName, GoatToolsPort);
        AZ::SystemTickBus::Handler::BusConnect();
        AZLOG_INFO("GOAT: offering agent state to a tool on port %u", static_cast<AZ::u32>(GoatToolsPort));
#endif
    }

    void GOATSystemComponent::StopRemoteDebug()
    {
        AZ::SystemTickBus::Handler::BusDisconnect();
    }

    void GOATSystemComponent::OnSystemTick()
    {
#if defined(ENABLE_REMOTE_TOOLS)
        auto* remoteTools = AzFramework::RemoteToolsInterface::Get();
        if (remoteTools == nullptr)
        {
            return;
        }

        const AzFramework::ReceivedRemoteToolsMessages* messages = remoteTools->GetReceivedMessages(GoatToolsKey);
        if (messages == nullptr || messages->empty())
        {
            return;
        }

        // Answered once however many times it was asked in one tick, so a tool that asks faster
        // than this process ticks cannot make it do the work twice.
        bool asked = false;
        AgentId watched;
        for (const AzFramework::RemoteToolsMessagePointer& message : *messages)
        {
            const auto* request = azrtti_cast<const GOATDebugRequest*>(message.get());
            if (request == nullptr)
            {
                continue;
            }
            if (request->m_protocolVersion != GoatDebugProtocolVersion)
            {
                AZLOG_WARN("GOAT: a tool asked in protocol %u and this build speaks %u, so it was ignored",
                    request->m_protocolVersion, GoatDebugProtocolVersion);
                continue;
            }
            asked = true;
            watched = request->GetWatched();
        }
        remoteTools->ClearReceivedMessages(GoatToolsKey);

        if (!asked)
        {
            return;
        }

        const AzFramework::RemoteToolsEndpointInfo tool = remoteTools->GetDesiredEndpoint(GoatToolsKey);
        if (!tool.IsValid() || tool.IsSelf())
        {
            return;
        }

        GOATDebugReply reply;
        reply.m_agents = SnapshotAgents(watched);
        remoteTools->SendRemoteToolsMessage(tool, reply);
#endif
    }

    void GOATSystemComponent::StartServices()
    {
        m_planStore = AZStd::make_unique<PlanStore>();
        m_blackboardSystem = AZStd::make_unique<BlackboardSystem>();
        m_actions = AZStd::make_unique<ActionStateRegistry>();
        m_backends = AZStd::make_unique<BackendRegistry>("backend");
        m_decisionBackends = AZStd::make_unique<DecisionBackendRegistry>("decision backend");
        m_nodeTypes = AZStd::make_unique<NodeTypeRegistry>();
        m_trees = AZStd::make_unique<TreeLibrary>();
        m_dispatch = AZStd::make_unique<LuaDispatch>();
        m_scriptContext = AZStd::make_unique<AgentScriptContext>();

        m_actions->RegisterAt(CoreActions::Wait, AZStd::make_unique<WaitAction>());
        m_actions->RegisterAt(CoreActions::RunScript, AZStd::make_unique<RunScriptAction>(*m_dispatch, *m_scriptContext));

        m_scripting = AZStd::make_unique<LuaNodeScripting>(*m_dispatch, *m_scriptContext);

        m_runtime = AZStd::make_unique<AgentRuntime>(
            *m_blackboardSystem, *m_actions, *m_backends, *m_scripting, *m_planStore);
        m_agents = AZStd::make_unique<AgentRegistry>(*m_runtime, *m_blackboardSystem, *m_dispatch);
        m_directors = AZStd::make_unique<DirectorRegistry>(*m_agents);

        // After the registry, because running a program inside a plan needs the agent whose
        // block it borrows and the runtime that hands it its context.
        m_actions->RegisterAt(CoreActions::Embed,
            AZStd::make_unique<EmbedAction>(
                [this](AgentId agent) { return m_agents->Find(agent); }, *m_runtime, *m_actions, m_programs));

        // Resolving a tree name needs the compiled programs, which live here, so the runtime is
        // handed the step rather than reaching back up for it.
        m_runtime->SetTreeSwitchHandler(
            [this](AgentRecord& agent)
            {
                ApplyTreeSwitch(agent);
            });

        InstallDirectorVocabulary();

        m_dispatch->ConfigurePlanBuilder(m_actions.get(), m_blackboardSystem.get(), m_planStore.get());
        m_vocabularyLoaded = false;
        m_dispatch->Connect();
    }

    void GOATSystemComponent::StopServices()
    {
        m_programs.clear();
        // Before the agents, because a director record is keyed by an agent handle.
        m_directors.reset();
        m_agents.reset();
        m_runtime.reset();
        m_scripting.reset();
        m_scriptContext.reset();
        if (m_dispatch != nullptr)
        {
            m_dispatch->Disconnect();
        }
        m_dispatch.reset();
        m_trees.reset();
        m_nodeTypes.reset();
        m_decisionBackends.reset();
        m_backends.reset();
        m_actions.reset();
        m_blackboardSystem.reset();

        // Last, because every agent's plan was a span into it.
        m_planStore.reset();
    }

    void GOATSystemComponent::InstallDirectorVocabulary()
    {
        AZ_Assert(m_directors != nullptr, "The director vocabulary needs the director registry");
        AZ_Assert(m_blackboardSystem != nullptr, "The director vocabulary needs the blackboard system");

        // Declared from C++ so a project never has to remember a .bbx for them; a missing one
        // would make every director tree fail to compile.
        m_directorKeys.Declare(*m_blackboardSystem);

        //! A leaf word whose name matches a verb runs that verb. The first required property is
        //! the one a bare string argument fills, so it goes first.
        const auto leaf = [](const char* name, const char* description)
        {
            NodeTypeDescriptor descriptor;
            descriptor.m_name = AZ::Name(name);
            descriptor.m_kind = NodeKind::Leaf;
            descriptor.m_op = NodeOp::Action;
            descriptor.m_category = "Director";
            descriptor.m_description = description;
            return descriptor;
        };

        const auto param = [](const char* name, BlackboardType type, bool isKey = false, bool required = false)
        {
            NodeParameter parameter;
            parameter.m_name = AZ::Name(name);
            parameter.m_type = type;
            parameter.m_isBlackboardKey = isKey;
            parameter.m_required = required;
            return parameter;
        };

        auto orderTree = leaf("order_tree", "Puts the agents in reach onto another tree");
        orderTree.m_parameters.push_back(param("tree", BlackboardType::Name, false, true));
        orderTree.m_parameters.push_back(param("key", BlackboardType::EntityId, true));
        orderTree.m_parameters.push_back(param("limit", BlackboardType::Float));

        auto orderInterrupt = leaf("order_interrupt", "Interrupts the agents in reach with another tree");
        orderInterrupt.m_parameters.push_back(param("tree", BlackboardType::Name, false, true));
        orderInterrupt.m_parameters.push_back(param("key", BlackboardType::EntityId, true));
        orderInterrupt.m_parameters.push_back(param("limit", BlackboardType::Float));

        auto orderBand = leaf("order_band", "Moves the agents in reach between pacing bands");
        orderBand.m_parameters.push_back(param("band", BlackboardType::Float, false, true));
        orderBand.m_parameters.push_back(param("key", BlackboardType::EntityId, true));

        auto orderValue = leaf("order_value", "Writes a variable, reaching whoever its scope says");
        orderValue.m_parameters.push_back(param("key", BlackboardType::Float, true, true));
        orderValue.m_parameters.push_back(param("value", BlackboardType::Float));
        orderValue.m_parameters.push_back(param("text", BlackboardType::Name));

        auto rebind = leaf("rebind_subtree", "Points a subtree slot at another tree");
        rebind.m_parameters.push_back(param("slot", BlackboardType::Name, false, true));
        rebind.m_parameters.push_back(param("key", BlackboardType::Name, true));

        const auto install = [this](AZStd::unique_ptr<IActionState> action, NodeTypeDescriptor descriptor)
        {
            const AZ::Name name = action->GetName();
            AZ_Assert(descriptor.m_name == name, "A leaf word and the verb it runs must share a name");

            if (m_actions->Register(AZStd::move(action)) == CoreActions::Invalid)
            {
                AZ_Error("GOAT", false, "Director verb '%s' could not be registered", name.GetCStr());
                return;
            }

            RegisterNodeType(AZStd::move(descriptor));
        };

        install(AZStd::make_unique<OrderTreeAction>(*m_directors, m_directorKeys), AZStd::move(orderTree));
        install(AZStd::make_unique<OrderInterruptAction>(*m_directors, m_directorKeys), AZStd::move(orderInterrupt));
        install(AZStd::make_unique<OrderBandAction>(*m_directors, m_directorKeys), AZStd::move(orderBand));
        install(AZStd::make_unique<OrderValueAction>(*m_directors, m_directorKeys), AZStd::move(orderValue));
        install(AZStd::make_unique<RebindSubtreeAction>(m_directorKeys), AZStd::move(rebind));
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

    bool GOATSystemComponent::RunScript(AZStd::string_view assetPath, const char* what)
    {
        const AZStd::string compiled = AZStd::string(assetPath) + ".luac";
        const AZStd::string source = AZStd::string(assetPath) + ".lua";
        const char* paths[] = { compiled.c_str(), source.c_str() };
        return RunFirstAvailable(paths, AZStd::size(paths), what);
    }

    void GOATSystemComponent::RegisterVocabularyScript(AZStd::string_view assetPath)
    {
        AZStd::string path(assetPath);
        if (AZStd::find(m_vocabularyScripts.begin(), m_vocabularyScripts.end(), path) != m_vocabularyScripts.end())
        {
            return;
        }

        m_vocabularyScripts.push_back(path);

        // A gem that arrives after the vocabulary loaded gets its words now rather than never.
        if (m_vocabularyLoaded)
        {
            AZ_Warning("GOAT", RunScript(path, "a gem's vocabulary"),
                "Could not load the vocabulary script at '%s'", path.c_str());
        }
    }

    void GOATSystemComponent::UnregisterVocabularyScript(AZStd::string_view assetPath)
    {
        const AZStd::string path(assetPath);
        m_vocabularyScripts.erase(
            AZStd::remove(m_vocabularyScripts.begin(), m_vocabularyScripts.end(), path), m_vocabularyScripts.end());
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

        // A backend gem's words load after the ones they are written in and before any user script.
        for (const AZStd::string& path : m_vocabularyScripts)
        {
            AZ_Warning("GOAT", RunScript(path, "a gem's vocabulary"),
                "Could not load the vocabulary script at '%s'", path.c_str());
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

    AZ::Outcome<void, AZStd::string> GOATSystemComponent::LoadProgram(const ProgramAsset& asset)
    {
        if (m_trees == nullptr)
        {
            return AZ::Failure(AZStd::string("The scripting services are not running"));
        }

        if (asset.m_name.empty())
        {
            return AZ::Failure(AZStd::string("A program asset has no name for agents to refer to it by"));
        }

        const AZ::Name name(asset.m_name);
        m_trees->Add(name, AZStd::shared_ptr<const AuthoredNode>(aznew AuthoredNode(asset.m_root)));
        m_assetPrograms.insert(name);
        return AZ::Success();
    }

    AZ::Outcome<void, AZStd::string> GOATSystemComponent::CompileNested(AgentProgram& program)
    {
        program.m_stateBytes = AlignState(program.m_backend->GetStateSize());

        size_t deepest = 0;
        for (const NestedProgram& nested : program.m_nested)
        {
            // A delegate naming something that is not a paradigm is a plain planner behind one
            // leaf. It compiles nothing and is looked up when the leaf runs, which is what lets
            // a backend declared in Lua register after the programs that name it have compiled.
            if (!nested.m_backend.IsEmpty() && m_decisionBackends->Find(nested.m_backend) == nullptr)
            {
                continue;
            }

            const IDecisionBackend* owner = nested.m_backend.IsEmpty()
                ? FindProgramBackend(nested.m_program)
                : m_decisionBackends->Find(nested.m_backend);

            if (owner == nullptr)
            {
                return AZ::Failure(AZStd::string::format(
                    "'%s' hands work to '%s', which no installed backend claims", program.m_name.GetCStr(),
                    nested.m_program.GetCStr()));
            }

            if (auto ran = CompileProgram(owner->GetName(), nested.m_program); !ran.IsSuccess())
            {
                return AZ::Failure(AZStd::string::format("'%s' hands work to '%s': %s", program.m_name.GetCStr(),
                    nested.m_program.GetCStr(), ran.GetError().c_str()));
            }

            const auto inner = m_programs.find(nested.m_program);
            AZ_Assert(inner != m_programs.end(), "A program that just compiled must be findable by name");

            // What the nested one watches, the host has to watch too, or the agent sleeps
            // through the very change the nested program is guarding on. What slots it was
            // compiled against the host has to own too, or a rebind cannot find whoever used it.
            for (size_t scope = 0; scope < inner->second->m_watchedScopes.size(); ++scope)
            {
                program.m_watchedScopes[scope] = program.m_watchedScopes[scope] || inner->second->m_watchedScopes[scope];
            }

            for (const AZ::Name& slot : inner->second->m_boundSlots)
            {
                if (AZStd::find(program.m_boundSlots.begin(), program.m_boundSlots.end(), slot) ==
                    program.m_boundSlots.end())
                {
                    program.m_boundSlots.push_back(slot);
                }
            }

            // The most any one of them needs, not the sum: two of these in different branches
            // run one at a time, and a chain of them is a stack.
            const size_t frame = nested.m_runsToCompletion ? NestedFrameBytes() : 0;
            deepest = AZStd::max(deepest, frame + inner->second->m_stateBytes);
        }

        program.m_stateBytes += deepest;
        return AZ::Success();
    }

    AZ::Outcome<void, AZStd::string> GOATSystemComponent::CompileProgram(
        const AZ::Name& backendName, const AZ::Name& programName)
    {
        if (AZStd::find(m_compiling.begin(), m_compiling.end(), programName) != m_compiling.end())
        {
            return AZ::Failure(AZStd::string::format(
                "'%s' hands work back to itself, directly or through another program", programName.GetCStr()));
        }

        if (m_compiling.size() >= MaxNestDepth)
        {
            return AZ::Failure(AZStd::string::format(
                "'%s' is %zu programs deep in handing work on, which is a loop rather than a chain",
                programName.GetCStr(), m_compiling.size()));
        }

        m_compiling.push_back(programName);
        const CompileFrame frame{ m_compiling };

        if (m_dispatch == nullptr || m_trees == nullptr)
        {
            return AZ::Failure(AZStd::string("The scripting services are not running"));
        }

        // An asset already holds its authored root, so only the Lua route has anything to emit.
        const bool fromAsset = m_assetPrograms.contains(programName);
        if (!fromAsset)
        {
            if (!EnsureVocabulary())
            {
                return AZ::Failure(AZStd::string(
                    "The GOAT authoring vocabulary is not loaded; check that goat/scripts/goat.lua reached the cache"));
            }

            auto emitted = m_dispatch->EmitTree(programName);
            if (!emitted.IsSuccess())
            {
                return AZ::Failure(emitted.TakeError());
            }

            m_trees->Add(programName, emitted.GetValue());
        }

        const AuthoredNode* root = m_trees->Find(programName);
        if (root == nullptr)
        {
            return AZ::Failure(AZStd::string::format(
                "No authored program named '%s' is registered", programName.GetCStr()));
        }

        IDecisionBackend* backend = m_decisionBackends->Find(backendName);
        if (backend == nullptr)
        {
            return AZ::Failure(AZStd::string::format(
                "No backend named '%s' is installed, so '%s' cannot be compiled", backendName.GetCStr(),
                programName.GetCStr()));
        }

        auto compiled = backend->Compile(programName, *root);
        if (!compiled.IsSuccess())
        {
            return AZ::Failure(compiled.TakeError());
        }

        AZStd::shared_ptr<AgentProgram> built = compiled.TakeValue();
        if (auto folded = CompileNested(*built); !folded.IsSuccess())
        {
            return AZ::Failure(folded.TakeError());
        }

        AZStd::shared_ptr<const AgentProgram> program = AZStd::move(built);
        m_programs[programName] = program;

        // An entity may have registered before this tree compiled, leaving its archetype holding
        // an empty slot under this name. Filling it here is what turns that declaration into
        // something the agent can be switched to, rather than leaving every agent sharing that
        // archetype refused for the rest of the level.
        for (const auto& archetype : m_archetypes)
        {
            archetype->Resolve(programName, program);
        }

        AZ_Assert(IsProgramCompiled(programName), "Compiling a tree must leave a program agents can be registered against");
        return AZ::Success();
    }

    bool GOATSystemComponent::IsProgramCompiled(const AZ::Name& programName) const
    {
        AZ_Assert(!programName.IsEmpty(), "A tree is always asked about by name");
        return m_programs.find(programName) != m_programs.end();
    }

    AgentId GOATSystemComponent::RegisterAgent(
        AZ::EntityId entity, const AZ::Name& backendName, AZStd::span<const AZ::Name> programs, size_t band,
        const AZ::Name& squad)
    {
        if (m_agents == nullptr || programs.empty())
        {
            AZ_Warning("GOAT", !programs.empty(), "Entity %s names no program to run",
                entity.ToString().c_str());
            return AgentId{};
        }

        AZ_Warning("GOAT", programs.size() <= MaxArchetypeTrees,
            "Entity %s lists %zu programs but an agent may declare %zu; the rest are ignored",
            entity.ToString().c_str(), programs.size(), MaxArchetypeTrees);

        // On the stack: registering a thousand agents must not mean a thousand throwaway lists.
        AZStd::fixed_vector<AZ::Name, MaxArchetypeTrees> declared;
        for (const AZ::Name& program : programs)
        {
            const bool listed = AZStd::find(declared.begin(), declared.end(), program) != declared.end();
            if (!listed && declared.size() < declared.capacity())
            {
                declared.push_back(program);
            }
        }

        // Program names share one namespace, so two entities can name the same one under
        // different backends. Running it under the wrong paradigm is worth saying out loud.
        const auto compiled = m_programs.find(declared.front());
        if (compiled != m_programs.end() && compiled->second->m_backend != nullptr &&
            compiled->second->m_backend->GetName() != backendName)
        {
            AZ_Error("GOAT", false, "Entity %s asks for backend '%s' but '%s' was compiled by '%s'",
                entity.ToString().c_str(), backendName.GetCStr(), declared.front().GetCStr(),
                compiled->second->m_backend->GetName().GetCStr());
            return AgentId{};
        }

        AZStd::shared_ptr<const AgentArchetype> archetype = AcquireArchetype(declared);
        if (archetype == nullptr)
        {
            return AgentId{};
        }

        return m_agents->Register(entity, AZStd::move(archetype), band, squad);
    }

    AZStd::shared_ptr<const AgentArchetype> GOATSystemComponent::AcquireArchetype(AZStd::span<const AZ::Name> trees)
    {
        // Shared rather than built per entity: ten thousand agents authored the same way then
        // hold one list of programs between them. The scan is over the number of distinct ways
        // agents are authored in a level, which is a handful.
        for (const auto& existing : m_archetypes)
        {
            if (existing->Matches(trees))
            {
                return existing;
            }
        }

        auto archetype = AZStd::shared_ptr<AgentArchetype>(aznew AgentArchetype());
        for (const AZ::Name& tree : trees)
        {
            const auto program = m_programs.find(tree);
            if (program == m_programs.end())
            {
                // Only the tree it starts in has to be compiled: a tree it merely declared may
                // still be waiting on a subtree binding, and refusing the agent for that would
                // stop it running the tree that is ready.
                if (tree == trees.front())
                {
                    AZ_Warning("GOAT", false, "Tree '%s' has not been compiled", tree.GetCStr());
                    return nullptr;
                }

                // The slot is taken regardless, so this archetype still describes the list it was
                // asked for and the next agent authored the same way shares it rather than
                // building another that will also match nothing. CompileProgram fills it in.
                AZ_Warning("GOAT", false, "Tree '%s' is declared but has not compiled yet", tree.GetCStr());
                archetype->Add(tree, nullptr);
                continue;
            }

            archetype->Add(tree, program->second);
        }

        m_archetypes.push_back(archetype);
        return archetype;
    }

    void GOATSystemComponent::UnregisterAgent(AgentId agent)
    {
        // Dropped with the agent, so the next agent to take this slot is told about its own
        // misconfiguration rather than silently inheriting this one's.
        for (auto it = m_reportedRefusals.begin(); it != m_reportedRefusals.end();)
        {
            it = (*it >> 32) == agent.GetIndex() ? m_reportedRefusals.erase(it) : AZStd::next(it);
        }

        if (m_agents != nullptr)
        {
            m_agents->Unregister(agent);
        }
    }

    bool GOATSystemComponent::RequestTreeSwitch(
        AgentId agent, const AZ::Name& treeName, TreeSwitchKind kind, AZ::u8 priority)
    {
        AZ_Assert(kind != TreeSwitchKind::None, "A switch request always says what it wants done");

        AgentRecord* record = m_agents != nullptr ? m_agents->Find(agent) : nullptr;
        if (record == nullptr)
        {
            AZ_Warning("GOAT", false, "Agent %u cannot change tree because it is not registered", agent.GetIndex());
            return false;
        }

        // The incumbent holds ties, so two directors of equal standing produce a stable outcome
        // rather than one that flips with whichever ticked last. A loser is dropped, never
        // queued: queueing would land it on the next window and undo the winner a tick later,
        // which is the thrash this whole design exists to prevent.
        if (record->m_pendingSwitch != TreeSwitchKind::None && priority <= record->m_pendingPriority)
        {
            AZLOG(GoatDirector,
                "GOAT: agent %u refused a priority %u command; a priority %u one is already pending",
                agent.GetIndex(), static_cast<AZ::u32>(priority), static_cast<AZ::u32>(record->m_pendingPriority));
            return false;
        }

        // Asked before the tree is looked up, because "the entity never said it could do that" is
        // what the author needs to hear. Compilation is global, so without this an order would
        // succeed or fail on whether some unrelated entity happened to list the same tree.
        const TreeSlot wantedSlot = kind != TreeSwitchKind::Pop ? record->FindTree(treeName) : InvalidTreeSlot;
        if (kind != TreeSwitchKind::Pop && wantedSlot == InvalidTreeSlot)
        {
            // Said once per agent and tree. A repertoire is fixed when the agent registers, so
            // the answer can never change, and a director asking every tick would bury every
            // other message in the log. The count still reaches a tree through director_refused.
            const AZ::u64 seen = (static_cast<AZ::u64>(agent.GetIndex()) << 32) | treeName.GetHash();
            const bool first = m_reportedRefusals.insert(seen).second;

            AZ_Error("GOAT", !first,
                "Agent %u cannot change to tree '%s': entity %s does not list it. Add it to that "
                "entity's tree list to allow the change",
                agent.GetIndex(), treeName.GetCStr(), record->m_entity.ToString().c_str());

            if (!first)
            {
                AZLOG(GoatDirector, "GOAT: agent %u was refused tree '%s' again",
                    agent.GetIndex(), treeName.GetCStr());
            }

            return false;
        }

        if (kind != TreeSwitchKind::Pop && !IsProgramCompiled(treeName))
        {
            AZ_Error("GOAT", false, "Agent %u cannot change to tree '%s', which is not compiled",
                agent.GetIndex(), treeName.GetCStr());
            return false;
        }

        if (kind == TreeSwitchKind::Pop && m_agents->PeekInterruptedTree(agent) == InvalidTreeSlot)
        {
            AZ_Warning("GOAT", false, "Agent %u has nothing to return to", agent.GetIndex());
            return false;
        }

        // Recorded rather than done: the caller may be Lua running inside this agent's own tick.
        record->m_pendingTree = wantedSlot;
        record->m_pendingSwitch = kind;
        record->m_pendingPriority = priority;
        return true;
    }

    void GOATSystemComponent::ApplyTreeSwitch(AgentRecord& agent)
    {
        const TreeSwitchKind kind = agent.m_pendingSwitch;
        TreeSlot wanted = agent.m_pendingTree;

        // Cleared first, so a switch that cannot be carried out is not retried every tick.
        agent.m_pendingSwitch = TreeSwitchKind::None;
        agent.m_pendingTree = InvalidTreeSlot;
        agent.m_pendingPriority = SelfSwitchPriority;

        if (kind == TreeSwitchKind::None || m_agents == nullptr)
        {
            return;
        }

        if (kind == TreeSwitchKind::Pop)
        {
            wanted = m_agents->PeekInterruptedTree(agent.m_id);
            if (wanted == InvalidTreeSlot)
            {
                return;
            }
        }

        if (!m_agents->ApplyTree(agent.m_id, wanted, kind == TreeSwitchKind::Push))
        {
            return;
        }

        if (kind == TreeSwitchKind::Set)
        {
            // Replacing outright abandons whatever was interrupted; there is no going back to a
            // tree the agent was told to stop running.
            while (m_agents->PeekInterruptedTree(agent.m_id) != InvalidTreeSlot)
            {
                m_agents->ForgetInterruptedTree(agent.m_id);
            }
        }
        else if (kind == TreeSwitchKind::Pop)
        {
            m_agents->ForgetInterruptedTree(agent.m_id);
        }
    }

    bool GOATSystemComponent::SetAgentTree(AgentId agent, const AZ::Name& treeName, AZ::u8 priority)
    {
        return RequestTreeSwitch(agent, treeName, TreeSwitchKind::Set, priority);
    }

    bool GOATSystemComponent::PushAgentTree(AgentId agent, const AZ::Name& treeName, AZ::u8 priority)
    {
        return RequestTreeSwitch(agent, treeName, TreeSwitchKind::Push, priority);
    }

    bool GOATSystemComponent::PopAgentTree(AgentId agent)
    {
        // A pop carries no priority: an agent returning to what it interrupted is finishing
        // something it started, not overruling anyone.
        return RequestTreeSwitch(agent, AZ::Name{}, TreeSwitchKind::Pop, SelfSwitchPriority);
    }

    AZ::Name GOATSystemComponent::GetAgentTree(AgentId agent) const
    {
        const AgentRecord* record = m_agents != nullptr ? m_agents->Find(agent) : nullptr;
        return record != nullptr ? record->GetTreeName() : AZ::Name{};
    }

    void GOATSystemComponent::LeaveSquad(AgentId agent)
    {
        if (m_agents != nullptr)
        {
            m_agents->LeaveSquad(agent);
        }
    }

    AZ::Name GOATSystemComponent::GetAgentSquad(AgentId agent) const
    {
        return m_blackboardSystem != nullptr ? m_blackboardSystem->GetSquad(agent) : AZ::Name{};
    }

    AZStd::vector<AgentId> GOATSystemComponent::GetAgents() const
    {
        return m_agents != nullptr ? m_agents->GetAgents() : AZStd::vector<AgentId>{};
    }

    AgentId GOATSystemComponent::FindAgent(AZ::EntityId entity) const
    {
        return m_agents != nullptr ? m_agents->FindByEntity(entity) : AgentId{};
    }

    AZ::EntityId GOATSystemComponent::GetAgentEntity(AgentId agent) const
    {
        const AgentRecord* record = m_agents != nullptr ? m_agents->Find(agent) : nullptr;
        return record != nullptr ? record->m_entity : AZ::EntityId{};
    }

    bool GOATSystemComponent::SetAgentBand(AgentId agent, size_t band)
    {
        AgentRecord* record = m_agents != nullptr ? m_agents->Find(agent) : nullptr;
        if (record == nullptr)
        {
            return false;
        }

        m_agents->SetBand(agent, band);
        return true;
    }

    size_t GOATSystemComponent::GetAgentBand(AgentId agent) const
    {
        const AgentRecord* record = m_agents != nullptr ? m_agents->Find(agent) : nullptr;
        return record != nullptr ? record->m_band : AgentRegistry::BandCount;
    }

    AZ::Outcome<size_t, AZStd::string> GOATSystemComponent::RebindSubtree(
        const AZ::Name& slot, const AZ::Name& treeName)
    {
        if (m_trees == nullptr)
        {
            return AZ::Failure(AZStd::string("The tree library is not running"));
        }

        AZ_Assert(!slot.IsEmpty(), "A subtree slot is always rebound by name");
        if (slot.IsEmpty() || treeName.IsEmpty())
        {
            return AZ::Failure(AZStd::string("A rebind names both a slot and the tree to bind to it"));
        }

        m_trees->Bind(slot, treeName);

        // Which programs used the slot is recorded on the program itself, by the compiler that
        // resolved it -- the only thing that could possibly know.
        AZStd::vector<AZ::Name> affected;
        for (const auto& [name, program] : m_programs)
        {
            if (program == nullptr)
            {
                continue;
            }

            const auto& slots = program->m_boundSlots;
            if (AZStd::find(slots.begin(), slots.end(), slot) != slots.end())
            {
                affected.push_back(name);
            }
        }

        // A tree whose slot was unbound failed to compile, so it is not in m_programs at all and
        // the scan above cannot see it. Retrying every declared but uncompiled tree is what lets
        // a slot be bound after the agents using it have already activated -- without this,
        // a tree with an unbound slot could never compile at any point.
        if (m_dispatch != nullptr)
        {
            for (const AZ::Name& declared : m_dispatch->GetDeclaredTreeNames())
            {
                if (!IsProgramCompiled(declared) &&
                    AZStd::find(affected.begin(), affected.end(), declared) == affected.end())
                {
                    affected.push_back(declared);
                }
            }
        }

        // A failed recompile leaves the old program in place, because CompileProgram fails before
        // it touches m_programs. A bad rebind therefore leaves the world running.
        size_t recompiled = 0;
        for (const AZ::Name& name : affected)
        {
            // Asked of the program when it has one and of its root word when it does not, which
            // is what lets a tree that never compiled because its slot was unbound compile now.
            const IDecisionBackend* backend = FindProgramBackend(name);
            if (backend == nullptr)
            {
                continue;
            }

            if (auto compiled = CompileProgram(backend->GetName(), name); compiled.IsSuccess())
            {
                ++recompiled;
            }
            else if (IsProgramCompiled(name))
            {
                // A tree that was running and now will not compile is a real failure; one that
                // was never compiled and still is not simply does not use this slot.
                return AZ::Failure(compiled.TakeError());
            }
        }

        // Agents already inside an affected tree keep the program they started on and finish it
        // coherently; they take the new one the next time they enter that tree.
        size_t stale = 0;
        for (const AgentId agent : GetAgents())
        {
            const AgentRecord* record = m_agents->Find(agent);
            const auto current = record != nullptr ? m_programs.find(record->GetTreeName()) : m_programs.end();
            if (record != nullptr && current != m_programs.end() && record->m_program != current->second.get())
            {
                ++stale;
            }
        }

        AZLOG(GoatDirector, "GOAT: slot '%s' now runs '%s'; %zu tree(s) recompiled, %zu agent(s) still on the old one",
            slot.GetCStr(), treeName.GetCStr(), recompiled, stale);

        return AZ::Success(recompiled);
    }

    bool GOATSystemComponent::RegisterDirector(AgentId director, const DirectorProfile& profile)
    {
        return m_directors != nullptr && m_directors->Register(director, profile);
    }

    void GOATSystemComponent::UnregisterDirector(AgentId director)
    {
        if (m_directors != nullptr)
        {
            m_directors->Unregister(director);
        }
    }

    size_t GOATSystemComponent::GetReachSize(AgentId director)
    {
        return m_directors != nullptr ? m_directors->Resolve(director).size() : 0;
    }

    AgentId GOATSystemComponent::GetInReach(AgentId director, size_t index)
    {
        if (m_directors == nullptr)
        {
            return AgentId{};
        }

        const auto& reach = m_directors->Resolve(director);
        return index < reach.size() ? reach[index] : AgentId{};
    }

    bool GOATSystemComponent::AttachDirectorFilter(AgentId director, IDirectorFilter& filter)
    {
        return m_directors != nullptr && m_directors->AttachFilter(director, filter);
    }

    void GOATSystemComponent::DetachDirectorFilter(AgentId director, IDirectorFilter& filter)
    {
        if (m_directors != nullptr)
        {
            m_directors->DetachFilter(director, filter);
        }
    }

    void GOATSystemComponent::JoinSquad(AgentId agent, const AZ::Name& squad)
    {
        // Through the registry, not straight to the blackboard system: joining has to re-arm the
        // agent's squad scoped guards, which were skipped when it registered.
        if (m_agents != nullptr)
        {
            m_agents->JoinSquad(agent, squad);
        }
    }

    AZ::Outcome<AZStd::shared_ptr<const AuthoredNode>, AZStd::string> GOATSystemComponent::EmitProgram(
        const AZ::Name& name)
    {
        if (m_dispatch == nullptr)
        {
            return AZ::Failure(AZStd::string("scripting is not running, so nothing can be emitted"));
        }
        return m_dispatch->EmitTree(name);
    }

    AZ::Name GOATSystemComponent::GetSubtreeBinding(const AZ::Name& slot) const
    {
        return m_trees != nullptr ? m_trees->GetBinding(slot) : AZ::Name{};
    }

    ActionStateId GOATSystemComponent::FindVerb(const AZ::Name& name) const
    {
        return m_actions != nullptr ? m_actions->FindId(name) : CoreActions::Invalid;
    }

    const NodeTypeDescriptor* GOATSystemComponent::FindNodeType(const AZ::Name& name) const
    {
        return m_nodeTypes != nullptr ? m_nodeTypes->Find(name) : nullptr;
    }

    IBackend* GOATSystemComponent::FindBackend(const AZ::Name& name) const
    {
        // A planner registered under the name wins, so naming a paradigm here is only ever the
        // fallback and nothing that resolves today starts resolving to something else.
        if (IBackend* planner = m_backends != nullptr ? m_backends->Find(name) : nullptr)
        {
            return planner;
        }

        const auto adapter = m_decisionAdapters.find(name);
        return adapter != m_decisionAdapters.end() ? adapter->second.get() : nullptr;
    }

    ActionResult GOATSystemComponent::CallBehavior(
        const AZ::Name& behavior, const char* phase, AgentId agent, float deltaTime)
    {
        if (m_dispatch == nullptr || m_scriptContext == nullptr)
        {
            return ActionResult::Failure;
        }

        m_scriptContext->Bind(agent, GetAgentEntity(agent), m_blackboardSystem.get());
        const ActionResult result = m_dispatch->CallBehavior(behavior, phase, agent, *m_scriptContext, deltaTime);
        m_scriptContext->Unbind();
        return result;
    }

    bool GOATSystemComponent::HasBehavior(const AZ::Name& behavior) const
    {
        return m_dispatch != nullptr && m_dispatch->HasBehavior(behavior);
    }

    bool GOATSystemComponent::MeasureBehavior(const AZ::Name& behavior, const char* phase, AgentId agent,
        AZStd::span<const float> values, float& outValue)
    {
        outValue = 0.0f;
        if (m_dispatch == nullptr || m_scriptContext == nullptr)
        {
            return false;
        }

        m_scriptContext->Bind(agent, GetAgentEntity(agent), m_blackboardSystem.get());
        const bool answered =
            m_dispatch->MeasureBehavior(behavior, phase, agent, *m_scriptContext, values, outValue);
        m_scriptContext->Unbind();
        return answered;
    }

    void GOATSystemComponent::WakeAgents(AZStd::span<const AgentId> agents)
    {
        if (m_agents != nullptr)
        {
            m_agents->Wake(agents);
        }
    }

    bool GOATSystemComponent::RegisterDecisionBackend(AZStd::unique_ptr<IDecisionBackend>& backend)
    {
        if (m_decisionBackends == nullptr || backend == nullptr)
        {
            return false;
        }

        IDecisionBackend* raw = backend.get();
        if (!m_decisionBackends->Register(AZStd::move(backend)))
        {
            return false;
        }

        // Which paradigm a program belongs to is answered by whoever gave its root word meaning.
        // That is the only thing the core can ask without naming a backend it must not know.
        for (const AZ::Name& word : raw->GetNodeTypes())
        {
            const auto claimed = m_nodeTypeOwners.find(word);
            AZ_Warning("GOAT", claimed == m_nodeTypeOwners.end(),
                "Backends '%s' and '%s' both claim the word '%s'; a program rooted in it cannot be placed",
                claimed != m_nodeTypeOwners.end() ? claimed->second->GetName().GetCStr() : "",
                raw->GetName().GetCStr(), word.GetCStr());
            m_nodeTypeOwners[word] = raw;
        }

        // A paradigm can also answer one leaf rather than a whole agent, so every one of them is
        // reachable from a delegate without being registered twice or knowing it was asked.
        if (m_agents != nullptr)
        {
            m_decisionAdapters[raw->GetName()] = AZStd::make_unique<DecisionBackendAdapter>(
                *raw, [this](AgentId agent) { return m_agents->Find(agent); }, m_programs);
        }

        return true;
    }

    void GOATSystemComponent::UnregisterDecisionBackend(const AZ::Name& name)
    {
        if (m_decisionBackends == nullptr)
        {
            return;
        }

        if (const IDecisionBackend* going = m_decisionBackends->Find(name))
        {
            for (const AZ::Name& word : going->GetNodeTypes())
            {
                const auto claimed = m_nodeTypeOwners.find(word);
                if (claimed != m_nodeTypeOwners.end() && claimed->second == going)
                {
                    m_nodeTypeOwners.erase(claimed);
                }
            }
        }

        m_decisionAdapters.erase(name);
        m_decisionBackends->Unregister(name);
    }

    IDecisionBackend* GOATSystemComponent::FindProgramBackend(const AZ::Name& programName) const
    {
        if (const auto compiled = m_programs.find(programName); compiled != m_programs.end())
        {
            return compiled->second->m_backend;
        }

        // Not compiled yet, so ask the word it is rooted in who gives that word meaning.
        const AuthoredNode* root = m_assetPrograms.contains(programName) && m_trees != nullptr
            ? m_trees->Find(programName)
            : nullptr;

        AZStd::shared_ptr<const AuthoredNode> emittedRoot;
        if (root == nullptr)
        {
            if (m_dispatch == nullptr)
            {
                return nullptr;
            }

            auto emitted = m_dispatch->EmitTree(programName);
            if (!emitted.IsSuccess() || emitted.GetValue() == nullptr)
            {
                return nullptr;
            }
            emittedRoot = emitted.TakeValue();
            root = emittedRoot.get();
        }

        const auto owner = m_nodeTypeOwners.find(AZ::Name(root->m_type));
        return owner != m_nodeTypeOwners.end() ? owner->second : nullptr;
    }

    IDecisionBackend* GOATSystemComponent::FindDecisionBackend(const AZ::Name& name) const
    {
        return m_decisionBackends != nullptr ? m_decisionBackends->Find(name) : nullptr;
    }

    AZStd::vector<AZ::Name> GOATSystemComponent::GetDecisionBackendNames() const
    {
        return m_decisionBackends != nullptr ? m_decisionBackends->GetNames() : AZStd::vector<AZ::Name>{};
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

    bool GOATSystemComponent::Snapshot(AgentId agent, bool withPosition, AgentSnapshot& outSnapshot) const
    {
        if (m_agents == nullptr)
        {
            return false;
        }

        AgentRecord* record = const_cast<AgentRegistry*>(m_agents.get())->Find(agent);
        if (record == nullptr)
        {
            return false;
        }

        const ActionRequest* action = record->m_machine.GetCurrentAction();

        outSnapshot = AgentSnapshot();
        outSnapshot.SetAgent(agent);
        outSnapshot.m_entity = record->m_entity;
        outSnapshot.m_program = record->m_program != nullptr ? record->m_program->m_name : AZ::Name();
        outSnapshot.m_backend = record->GetBackend() != nullptr ? record->GetBackend()->GetName() : AZ::Name();
        outSnapshot.m_squad = GetAgentSquad(agent);
        outSnapshot.m_action = action != nullptr && m_actions->Find(action->m_action) != nullptr
            ? m_actions->Find(action->m_action)->GetName()
            : AZ::Name("idle");
        outSnapshot.m_band = record->m_band;
        outSnapshot.m_step = aznumeric_cast<AZ::u32>(record->m_machine.GetStepIndex());
        outSnapshot.m_planSize = aznumeric_cast<AZ::u32>(record->m_machine.GetPlanSize());
        outSnapshot.m_elapsed = record->m_machine.GetElapsed();
        outSnapshot.m_interrupted = aznumeric_cast<AZ::u32>(record->m_treeStack.size());

        // Only the backend that wrote the agent's state block can read it, so where the agent
        // is inside its program is the one thing here the core cannot work out for itself.
        // Left out unless asked for: it walks the agent's state and allocates a path, which is
        // not something to do for every agent in a level when one of them is being watched.
        if (withPosition && record->GetBackend() != nullptr && record->m_program != nullptr)
        {
            record->GetBackend()->DescribePosition(*record->m_program, record->GetState(),
                record->m_machine.HasPlan() ? record->m_machine.GetStepIndex() : NoRunningStep,
                outSnapshot.m_activePath);
        }
        return true;
    }

    bool GOATSystemComponent::SnapshotAgent(AgentId agent, AgentSnapshot& outSnapshot) const
    {
        return Snapshot(agent, true, outSnapshot);
    }

    AZStd::vector<AgentSnapshot> GOATSystemComponent::SnapshotAgents(AgentId detail) const
    {
        AZStd::vector<AgentSnapshot> snapshots;
        if (m_agents == nullptr)
        {
            return snapshots;
        }

        const AZStd::vector<AgentId> agents = m_agents->GetAgents();
        snapshots.reserve(agents.size());
        for (const AgentId agent : agents)
        {
            AgentSnapshot snapshot;
            if (Snapshot(agent, agent == detail, snapshot))
            {
                snapshots.push_back(AZStd::move(snapshot));
            }
        }
        return snapshots;
    }

    AZStd::string GOATSystemComponent::DescribeAgent(AgentId agent) const
    {
        if (m_agents == nullptr)
        {
            return "the agent system is not running";
        }

        // Built from the same snapshot the tools read, so the console and a panel can never
        // disagree about what an agent is doing.
        AgentSnapshot snapshot;
        if (!SnapshotAgent(agent, snapshot))
        {
            return "no such agent";
        }

        return AZStd::string::format(
            "program '%s' on backend '%s' (%u interrupted) band %u action '%s' step %u of %u elapsed %.2fs",
            snapshot.m_program.IsEmpty() ? "<none>" : snapshot.m_program.GetCStr(),
            snapshot.m_backend.IsEmpty() ? "<none>" : snapshot.m_backend.GetCStr(),
            snapshot.m_interrupted, static_cast<AZ::u32>(snapshot.m_band), snapshot.m_action.GetCStr(),
            snapshot.m_step, snapshot.m_planSize, snapshot.m_elapsed);
    }

    namespace
    {
        //! Reads an entity id from a console argument, reporting what it could not read.
        //! Five console commands were doing this letter for letter.
        bool ReadEntity(const AZ::ConsoleCommandContainer& arguments, size_t index, AZ::EntityId& outEntity)
        {
            AZ::u64 rawEntityId = 0;
            if (index >= arguments.size() || !AZ::ConsoleTypeHelpers::ToValue(rawEntityId, arguments[index]))
            {
                AZLOG_INFO("could not read '%.*s' as an entity id",
                    index < arguments.size() ? aznumeric_cast<int>(arguments[index].size()) : 0,
                    index < arguments.size() ? arguments[index].data() : "");
                return false;
            }

            outEntity = AZ::EntityId(rawEntityId);
            return true;
        }

        //! What is narrowing a director, by type, for console output.
        AZStd::string DescribeFilters(const AZStd::vector<const IDirectorFilter*>& filters)
        {
            if (filters.empty())
            {
                return "nothing";
            }

            AZStd::string described;
            for (const IDirectorFilter* filter : filters)
            {
                described += described.empty() ? "" : ", ";
                described += filter->RTTI_GetTypeName();
            }
            return described;
        }
    } // namespace

    void GOATSystemComponent::SetAgentTreeCommand(const AZ::ConsoleCommandContainer& arguments)
    {
        if (arguments.size() < 2 || m_agents == nullptr)
        {
            AZLOG_INFO("usage: GOATSystemComponent.SetAgentTreeCommand <entityId> <tree>");
            return;
        }

        AZ::EntityId wantedEntity;
        if (!ReadEntity(arguments, 0, wantedEntity))
        {
            return;
        }

        // Named on its own line: AZ::Name treeName(AZStd::string(...)) parses as a declaration.
        const AZStd::string wantedTree(arguments[1]);
        const AZ::Name treeName(wantedTree);

        const AgentId agent = m_agents->FindByEntity(wantedEntity);
        if (agent.IsNull())
        {
            AZLOG_INFO("no agent is running on entity %s", wantedEntity.ToString().c_str());
            return;
        }

        // From the console, at the highest priority: someone typing a command is overruling
        // whatever any director decided, which is what a debugging command is for.
        AZLOG_INFO("%s agent %u to tree '%s'",
            SetAgentTree(agent, treeName, AZStd::numeric_limits<AZ::u8>::max()) ? "moving" : "could not move",
            agent.GetIndex(), treeName.GetCStr());
    }

    void GOATSystemComponent::SetAgentBandCommand(const AZ::ConsoleCommandContainer& arguments)
    {
        if (arguments.size() < 2 || m_agents == nullptr)
        {
            AZLOG_INFO("usage: GOATSystemComponent.SetAgentBandCommand <entityId> <band>");
            return;
        }

        AZ::u32 wantedBand = 0;
        if (!AZ::ConsoleTypeHelpers::ToValue(wantedBand, arguments[1]))
        {
            AZLOG_INFO("could not read '%.*s' as a band",
                aznumeric_cast<int>(arguments[1].size()), arguments[1].data());
            return;
        }

        AZ::EntityId wantedEntity;
        if (!ReadEntity(arguments, 0, wantedEntity))
        {
            return;
        }

        const AgentId agent = m_agents->FindByEntity(wantedEntity);
        if (agent.IsNull())
        {
            AZLOG_INFO("no agent is running on entity %s", wantedEntity.ToString().c_str());
            return;
        }

        // Band is a pacing lever rather than a decision, so it applies at once instead of being
        // recorded like a tree switch: nothing is holding a reference to the band this tick.
        AZLOG_INFO("%s agent %u to band %u",
            SetAgentBand(agent, static_cast<size_t>(wantedBand)) ? "moving" : "could not move",
            agent.GetIndex(), wantedBand);
    }

    void GOATSystemComponent::ListDirectors([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
    {
        if (m_directors == nullptr)
        {
            return;
        }

        for (const AgentId director : m_directors->GetDirectors())
        {
            const DirectorProfile* profile = m_directors->FindProfile(director);
            if (profile == nullptr)
            {
                continue;
            }

            AZLOG_INFO("director %u: entity %s | priority %u | narrowed by %s | governs %zu",
                director.GetIndex(), GetAgentEntity(director).ToString().c_str(),
                static_cast<AZ::u32>(profile->m_priority),
                DescribeFilters(m_directors->GetFilters(director)).c_str(),
                m_directors->Resolve(director).size());
        }
    }

    void GOATSystemComponent::DumpDirector(const AZ::ConsoleCommandContainer& arguments)
    {
        if (arguments.empty() || m_directors == nullptr)
        {
            AZLOG_INFO("usage: GOATSystemComponent.DumpDirector <entityId>");
            return;
        }

        AZ::EntityId wantedEntity;
        if (!ReadEntity(arguments, 0, wantedEntity))
        {
            return;
        }

        const AgentId director = FindAgent(wantedEntity);
        if (director.IsNull() || m_directors->FindProfile(director) == nullptr)
        {
            AZLOG_INFO("no director is running on entity %s", wantedEntity.ToString().c_str());
            return;
        }

        AZLOG_INFO("narrowed by %s", DescribeFilters(m_directors->GetFilters(director)).c_str());

        for (const AgentId agent : m_directors->Resolve(director))
        {
            AZLOG_INFO("  agent %u: entity %s | %s", agent.GetIndex(),
                GetAgentEntity(agent).ToString().c_str(), DescribeAgent(agent).c_str());
        }
    }

    void GOATSystemComponent::ListSquads([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
    {
        if (m_blackboardSystem == nullptr)
        {
            return;
        }

        for (const AZ::Name& name : m_blackboardSystem->GetSquadNames())
        {
            AZLOG_INFO("squad: %s", name.GetCStr());
        }
    }

    void GOATSystemComponent::RebindSubtreeCommand(const AZ::ConsoleCommandContainer& arguments)
    {
        if (arguments.size() < 2)
        {
            AZLOG_INFO("usage: GOATSystemComponent.RebindSubtreeCommand <slot> <tree>");
            return;
        }

        const AZStd::string slot(arguments[0]);
        const AZStd::string tree(arguments[1]);

        auto rebound = RebindSubtree(AZ::Name(slot), AZ::Name(tree));
        if (!rebound.IsSuccess())
        {
            AZLOG_INFO("%s", rebound.GetError().c_str());
            return;
        }

        AZLOG_INFO("slot '%s' now runs '%s'; %zu tree(s) recompiled",
            slot.c_str(), tree.c_str(), rebound.GetValue());
    }

    namespace
    {
        //! Reads the optional trailing entity argument shared by the variable commands.
        //! An agent or squad scoped variable is reached *through* an agent, so a command that
        //! names none can only address a global.
        AgentId ReadOptionalAgent(
            const IAgentSystem& agents, const AZ::ConsoleCommandContainer& arguments, size_t index)
        {
            if (arguments.size() <= index)
            {
                return AgentId{};
            }

            AZ::u64 rawEntityId = 0;
            if (!AZ::ConsoleTypeHelpers::ToValue(rawEntityId, arguments[index]))
            {
                return AgentId{};
            }

            return agents.FindAgent(AZ::EntityId(rawEntityId));
        }
    } // namespace

    void GOATSystemComponent::SetVariable(const AZ::ConsoleCommandContainer& arguments)
    {
        if (arguments.size() < 2 || m_blackboardSystem == nullptr)
        {
            AZLOG_INFO("usage: GOATSystemComponent.SetVariable <name> <value> [entityId]");
            return;
        }

        const AZStd::string name(arguments[0]);
        const AZStd::string value(arguments[1]);
        const BlackboardKey key = m_blackboardSystem->FindKey(AZ::Name(name));

        if (!key.IsValid())
        {
            AZLOG_INFO("no variable named '%s' is declared", name.c_str());
            return;
        }

        const AgentId through = ReadOptionalAgent(*this, arguments, 2);
        if (key.GetScope() != BlackboardScope::Global && through.IsNull())
        {
            AZLOG_INFO("'%s' is %s scoped, so it needs an entity id to reach it through",
                name.c_str(), ToString(key.GetScope()));
            return;
        }

        double number = 0.0;
        const bool numeric = AZ::ConsoleTypeHelpers::ToValue(number, arguments[1]);

        bool written = false;
        switch (key.GetType())
        {
        case BlackboardType::Bool:
            written = m_blackboardSystem->Set<bool>(key, numeric && number != 0.0, through);
            break;
        case BlackboardType::Int:
            written = m_blackboardSystem->Set<AZ::s64>(key, static_cast<AZ::s64>(number), through);
            break;
        case BlackboardType::Float:
            written = m_blackboardSystem->Set<float>(key, static_cast<float>(number), through);
            break;
        case BlackboardType::Name:
            written = m_blackboardSystem->Set<AZ::Name>(key, AZ::Name(value), through);
            break;
        default:
            AZLOG_INFO("'%s' is a %s, which this command cannot write", name.c_str(), ToString(key.GetType()));
            return;
        }

        AZLOG_INFO("%s '%s' (%s %s) to %s", written ? "wrote" : "could not write", name.c_str(),
            ToString(key.GetScope()), ToString(key.GetType()), value.c_str());
    }

    void GOATSystemComponent::DumpVariable(const AZ::ConsoleCommandContainer& arguments)
    {
        if (arguments.empty() || m_blackboardSystem == nullptr)
        {
            AZLOG_INFO("usage: GOATSystemComponent.DumpVariable <name> [entityId]");
            return;
        }

        const AZStd::string name(arguments[0]);
        const BlackboardKey key = m_blackboardSystem->FindKey(AZ::Name(name));
        if (!key.IsValid())
        {
            AZLOG_INFO("no variable named '%s' is declared", name.c_str());
            return;
        }

        const AgentId through = ReadOptionalAgent(*this, arguments, 1);

        switch (key.GetType())
        {
        case BlackboardType::Bool:
        {
            const bool* value = m_blackboardSystem->Find<bool>(key, through);
            AZLOG_INFO("%s = %s", name.c_str(), value == nullptr ? "<no storage>" : (*value ? "true" : "false"));
            break;
        }
        case BlackboardType::Int:
        {
            const AZ::s64* value = m_blackboardSystem->Find<AZ::s64>(key, through);
            if (value == nullptr) { AZLOG_INFO("%s = <no storage>", name.c_str()); }
            else { AZLOG_INFO("%s = %lld", name.c_str(), static_cast<long long>(*value)); }
            break;
        }
        case BlackboardType::Float:
        {
            const float* value = m_blackboardSystem->Find<float>(key, through);
            if (value == nullptr) { AZLOG_INFO("%s = <no storage>", name.c_str()); }
            else { AZLOG_INFO("%s = %.3f", name.c_str(), *value); }
            break;
        }
        case BlackboardType::Name:
        {
            const AZ::Name* value = m_blackboardSystem->Find<AZ::Name>(key, through);
            AZLOG_INFO("%s = '%s'", name.c_str(), value == nullptr ? "<no storage>" : value->GetCStr());
            break;
        }
        case BlackboardType::EntityId:
        {
            const AZ::EntityId* value = m_blackboardSystem->Find<AZ::EntityId>(key, through);
            AZLOG_INFO("%s = %s", name.c_str(),
                value == nullptr ? "<no storage>" : value->ToString().c_str());
            break;
        }
        default:
            AZLOG_INFO("%s is a %s, which this command cannot show", name.c_str(), ToString(key.GetType()));
            break;
        }
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
            const IDecisionBackend* backend = program->second->m_backend;
            AZLOG_INFO("program: %s (%s, %zu bound slot(s))", name.GetCStr(),
                backend != nullptr ? backend->GetName().GetCStr() : "no backend",
                program->second->m_boundSlots.size());
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

        AZ::EntityId wanted;
        if (!ReadEntity(arguments, 0, wanted))
        {
            return;
        }

        const AgentId agent = m_agents->FindByEntity(wanted);
        const AgentRecord* record = agent.IsNull() ? nullptr : m_agents->Find(agent);
        if (record == nullptr)
        {
            AZLOG_INFO("no agent is running on entity %s", wanted.ToString().c_str());
            return;
        }

        AZLOG_INFO("agent %u: %s", agent.GetIndex(), DescribeAgent(agent).c_str());

        // Printed because a refused order is most often a tree the entity never listed, and this
        // is the only place that list can be seen from.
        AZStd::string mayRun;
        for (size_t slot = 0; slot < record->m_archetype->Size(); ++slot)
        {
            mayRun += mayRun.empty() ? "" : ", ";
            mayRun += record->m_archetype->GetName(static_cast<TreeSlot>(slot)).GetCStr();
        }

        AZLOG_INFO("  may run: %s", mayRun.c_str());

        // The same path the graph editor lights up, in text, so the two can be checked against
        // each other and so a launcher with no editor attached can still be asked.
        AgentSnapshot snapshot;
        if (SnapshotAgent(agent, snapshot) && !snapshot.m_activePath.empty())
        {
            AZStd::string running;
            for (const ProgramNodeRef& step : snapshot.m_activePath)
            {
                running += running.empty() ? "" : " > ";
                running += step.m_program.GetCStr();
                for (const AZ::u16 index : step.m_path)
                {
                    running += AZStd::string::format("[%u]", index);
                }
            }
            AZLOG_INFO("  running: %s", running.c_str());
        }
    }

    void GOATSystemComponent::OnCatalogLoaded([[maybe_unused]] const char* catalogFile)
    {
        // The catalog is up now, so the vocabulary can finally be found.
        EnsureVocabulary();
    }

    void GOATSystemComponent::ReloadVocabulary([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
    {
        // The one place dropping baked plans is right: a reload means every declaration may have
        // changed, so what agents are running is stale by definition.
        if (m_dispatch != nullptr)
        {
            m_dispatch->GetPlanBuilder().ClearBaked();
        }

        // Forces a fresh attempt, so a failure at activation can be diagnosed once the
        // asset catalog is up rather than only being reported as a warning at startup.
        m_vocabularyLoaded = false;
        if (EnsureVocabulary())
        {
            // Baked again from the declarations Lua still holds. Re-running the files cannot
            // restore what clearing just dropped: the script system executes a chunk once and
            // hands back a cached result ever after, so only re-baking puts the steps back.
            if (m_dispatch != nullptr)
            {
                m_dispatch->BakePlans();
            }

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
        if (AZ::Data::AssetManager::Instance().GetHandler(azrtti_typeid<BlackboardAsset>()) == nullptr)
        {
            auto handler = AZStd::make_unique<BlackboardAssetHandler>();
            handler->Register();
            m_assetHandlers.emplace_back(AZStd::move(handler));
        }

        if (AZ::Data::AssetManager::Instance().GetHandler(azrtti_typeid<ProgramAsset>()) == nullptr)
        {
            auto handler = AZStd::make_unique<ProgramAssetHandler>();
            handler->Register();
            m_assetHandlers.emplace_back(AZStd::move(handler));
        }
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
