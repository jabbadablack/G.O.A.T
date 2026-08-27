#include "GOATSystemComponent.h"

#include <Core/Assets/BlackboardAssetHandler.h>
#include <Core/Actions/RunScriptAction.h>
#include <Core/Actions/WaitAction.h>
#include <Core/Director/DirectorActions.h>
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
    }

    void GOATSystemComponent::Deactivate()
    {
        if (AgentSystemInterface::Get() == this)
        {
            AgentSystemInterface::Unregister(this);
        }

        GOATBackendRequestBus::Handler::BusDisconnect();
        AzFramework::AssetCatalogEventBus::Handler::BusDisconnect();
        UnregisterAssetHandlers();
        StopServices();
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
        m_reachFilters = AZStd::make_unique<ReachFilterRegistry>("reach filter");
        m_directors = AZStd::make_unique<DirectorRegistry>(*m_agents, *m_blackboardSystem, *m_reachFilters);

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
        m_reachFilters.reset();
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

        auto program = AZStd::shared_ptr<const DecisionProgram>(aznew DecisionProgram(AZStd::move(compiled.GetValue())));
        m_programs[treeName] = program;

        // An entity may have registered before this tree compiled, leaving its archetype holding
        // an empty slot under this name. Filling it here is what turns that declaration into
        // something the agent can be switched to, rather than leaving every agent sharing that
        // archetype refused for the rest of the level.
        for (const auto& archetype : m_archetypes)
        {
            archetype->Resolve(treeName, program);
        }

        AZ_Assert(IsTreeCompiled(treeName), "Compiling a tree must leave a program agents can be registered against");
        return AZ::Success();
    }

    bool GOATSystemComponent::IsTreeCompiled(const AZ::Name& treeName) const
    {
        AZ_Assert(!treeName.IsEmpty(), "A tree is always asked about by name");
        return m_programs.find(treeName) != m_programs.end();
    }

    AgentId GOATSystemComponent::RegisterAgent(
        AZ::EntityId entity, const AZ::Name& treeName, size_t band, const AZ::Name& squad,
        AZStd::span<const AZ::Name> repertoire)
    {
        if (m_agents == nullptr)
        {
            return AgentId{};
        }

        // The tree it starts in comes first, whatever order the entity listed them in, because
        // slot zero is where an agent begins. Anything else it declared follows.
        // On the stack: registering a thousand agents must not mean a thousand throwaway lists.
        AZStd::fixed_vector<AZ::Name, MaxArchetypeTrees> trees;
        trees.push_back(treeName);
        for (const AZ::Name& tree : repertoire)
        {
            if (tree != treeName && trees.size() < trees.capacity())
            {
                trees.push_back(tree);
            }
        }

        AZ_Warning("GOAT", repertoire.size() < MaxArchetypeTrees,
            "Entity %s lists %zu trees but an agent may declare %zu; the rest are ignored",
            entity.ToString().c_str(), repertoire.size(), MaxArchetypeTrees);

        AZStd::shared_ptr<const AgentArchetype> archetype = AcquireArchetype(trees);
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
                // building another that will also match nothing. CompileTree fills it in.
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

        if (kind != TreeSwitchKind::Pop && !IsTreeCompiled(treeName))
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
                if (!IsTreeCompiled(declared) &&
                    AZStd::find(affected.begin(), affected.end(), declared) == affected.end())
                {
                    affected.push_back(declared);
                }
            }
        }

        // A failed recompile leaves the old program in place, because CompileTree fails before it
        // touches m_programs. A bad rebind therefore leaves the world running.
        size_t recompiled = 0;
        for (const AZ::Name& name : affected)
        {
            if (auto compiled = CompileTree(name); compiled.IsSuccess())
            {
                ++recompiled;
            }
            else if (IsTreeCompiled(name))
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

    bool GOATSystemComponent::RegisterReachFilter(AZStd::unique_ptr<IReachFilter> filter)
    {
        return m_reachFilters != nullptr && m_reachFilters->Register(AZStd::move(filter));
    }

    void GOATSystemComponent::UnregisterReachFilter(const AZ::Name& name)
    {
        if (m_reachFilters != nullptr)
        {
            m_reachFilters->Unregister(name);
        }
    }

    AZStd::vector<AZ::Name> GOATSystemComponent::GetReachFilterNames() const
    {
        return m_reachFilters != nullptr ? m_reachFilters->GetNames() : AZStd::vector<AZ::Name>{};
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

    bool GOATSystemComponent::RegisterDecisionBackend(AZStd::unique_ptr<IDecisionBackend>& backend)
    {
        return m_decisionBackends != nullptr && m_decisionBackends->Register(AZStd::move(backend));
    }

    void GOATSystemComponent::UnregisterDecisionBackend(const AZ::Name& name)
    {
        if (m_decisionBackends != nullptr)
        {
            m_decisionBackends->Unregister(name);
        }
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
            "tree '%s' (%zu interrupted) band %u node %u action '%s' step %zu of %zu elapsed %.2fs",
            record->m_program != nullptr ? record->m_program->m_name.GetCStr() : "<none>",
            record->m_treeStack.size(), static_cast<AZ::u32>(record->m_band),
            record->m_cursor.GetActiveLeaf(), verb.GetCStr(),
            record->m_machine.GetStepIndex(), record->m_machine.GetPlanSize(), record->m_machine.GetElapsed());
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

            const DirectorReach& reach = profile->m_reach;
            AZLOG_INFO(
                "director %u: entity %s | priority %u | squad '%s' tree '%s' radius %.1f filter '%s' | governs %zu",
                director.GetIndex(), GetAgentEntity(director).ToString().c_str(),
                static_cast<AZ::u32>(profile->m_priority),
                reach.m_squad.IsEmpty() ? "any" : reach.m_squad.GetCStr(),
                reach.m_tree.IsEmpty() ? "any" : reach.m_tree.GetCStr(),
                reach.m_radius, reach.m_filter.IsEmpty() ? "none" : reach.m_filter.GetCStr(),
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

        for (const AgentId agent : m_directors->Resolve(director))
        {
            AZLOG_INFO("  agent %u: entity %s | %s", agent.GetIndex(),
                GetAgentEntity(agent).ToString().c_str(), DescribeAgent(agent).c_str());
        }
    }

    void GOATSystemComponent::ListReachFilters([[maybe_unused]] const AZ::ConsoleCommandContainer& arguments)
    {
        for (const AZ::Name& name : GetReachFilterNames())
        {
            AZLOG_INFO("reach filter: %s", name.GetCStr());
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
