#pragma once

#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/Domain/ActionState.h>
#include <GOAT/Assets/ProgramAsset.h>
#include <GOAT/Domain/AgentDebug.h>
#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/DirectorProfile.h>
#include <GOAT/Domain/NodeType.h>
#include <GOAT/Interfaces/IDirectorFilter.h>
#include <GOAT/GOATTypeIds.h>
#include <GOAT/Interfaces/IActionState.h>
#include <GOAT/Interfaces/IBackend.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/Script/ScriptAsset.h>
#include <AzCore/std/containers/span.h>
#include <AzCore/std/smart_ptr/shared_ptr.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>
#include <AzCore/std/string/string_view.h>

namespace GOAT
{
    //! Turns entities into agents and lets modules and backends extend the vocabulary.
    //! This is the whole surface a game or an extension gem needs.
    class IAgentSystem
    {
    public:
        AZ_RTTI(IAgentSystem, IAgentSystemTypeId);

        virtual ~IAgentSystem() = default;

        //! Runs a Lua script, registering whatever behaviours, backends and trees it declares.
        virtual bool LoadScript(const AZ::Data::Asset<AZ::ScriptAsset>& asset) = 0;

        //! Declares the variables a blackboard asset holds. Duplicate names fail.
        virtual AZ::Outcome<void, AZStd::string> LoadBlackboard(const BlackboardAsset& asset) = 0;

        //! Declares the program an asset holds, under the name the asset carries.
        //! The graph editor's output enters here, where Lua's enters through EmitProgram.
        virtual AZ::Outcome<void, AZStd::string> LoadProgram(const ProgramAsset& asset) = 0;

        //! Compiles a declared program through a named backend. Always recompiles, which is
        //! what makes it the way to pick up a rebound dynamic subtree.
        virtual AZ::Outcome<void, AZStd::string> CompileProgram(
            const AZ::Name& backendName, const AZ::Name& programName) = 0;

        //! True when a program is already compiled and agents can be registered against it.
        //! Program names share one namespace, like blackboard variable names.
        virtual bool IsProgramCompiled(const AZ::Name& programName) const = 0;

        //! Registers an entity as an agent, run by a named backend.
        //! @param programs what it may run. The first is what it starts in, and anything outside
        //! the list is refused, so an order cannot put an agent somewhere its author did not say.
        //! @param squad joined as part of registering, because an agent's guards are armed here
        //! and a squad scoped guard can only watch storage that already exists.
        virtual AgentId RegisterAgent(
            AZ::EntityId entity, const AZ::Name& backendName, AZStd::span<const AZ::Name> programs, size_t band,
            const AZ::Name& squad) = 0;

        //! Removes an agent and everything held for it.
        virtual void UnregisterAgent(AgentId agent) = 0;

        //! Puts an agent onto another of its trees, ending whatever it was running first.
        //! Refused when the tree is not one the entity declared it may run.
        //! Replaces outright and forgets anything it had interrupted.
        //! @param priority whoever is asking. A higher priority command replaces one still
        //! waiting to be applied; a lower one arriving after it is dropped.
        virtual bool SetAgentTree(
            AgentId agent, const AZ::Name& treeName, AZ::u8 priority = SelfSwitchPriority) = 0;

        //! Interrupts an agent with another tree, remembering what to come back to.
        virtual bool PushAgentTree(
            AgentId agent, const AZ::Name& treeName, AZ::u8 priority = SelfSwitchPriority) = 0;

        //! Returns an agent to the tree it last interrupted. False when it interrupted nothing.
        virtual bool PopAgentTree(AgentId agent) = 0;

        //! Which tree an agent is running, or an empty name when it is not registered.
        virtual AZ::Name GetAgentTree(AgentId agent) const = 0;

        //! Puts an agent in a named squad, creating that squad on the first join.
        virtual void JoinSquad(AgentId agent, const AZ::Name& squad) = 0;

        //! Takes an agent out of its squad, destroying that squad on the last leave.
        virtual void LeaveSquad(AgentId agent) = 0;

        //! The squad an agent is in, or an empty name when it is in none.
        virtual AZ::Name GetAgentSquad(AgentId agent) const = 0;

        //! Every registered agent. The roster a director's reach is filtered from.
        virtual AZStd::vector<AgentId> GetAgents() const = 0;

        //! The agent driving an entity, or a null handle when it drives none.
        virtual AgentId FindAgent(AZ::EntityId entity) const = 0;

        //! The entity an agent drives, or an invalid id when it is not registered.
        virtual AZ::EntityId GetAgentEntity(AgentId agent) const = 0;

        //! Moves an agent between pacing bands, which is the level of detail lever.
        virtual bool SetAgentBand(AgentId agent, size_t band) = 0;
        virtual size_t GetAgentBand(AgentId agent) const = 0;

        //! Points a subtree slot at another tree and recompiles every tree that used it.
        //! Agents already running an affected tree keep the program they started on, so a rebind
        //! never rewrites a tree under an agent mid action; they pick the new one up next time
        //! they enter that tree.
        //! @return how many trees were recompiled.
        virtual AZ::Outcome<size_t, AZStd::string> RebindSubtree(
            const AZ::Name& slot, const AZ::Name& treeName) = 0;

        //! Makes an agent a director. It governs every other agent until a filter narrows it.
        //! False when it already is one.
        virtual bool RegisterDirector(AgentId director, const DirectorProfile& profile) = 0;
        virtual void UnregisterDirector(AgentId director) = 0;

        //! How many agents a director governs, and which. Zero and null for a non director.
        virtual size_t GetReachSize(AgentId director) = 0;
        virtual AgentId GetInReach(AgentId director, size_t index) = 0;

        //! Narrows what a director governs. The filter is not owned by the core.
        virtual bool AttachDirectorFilter(AgentId director, IDirectorFilter& filter) = 0;
        virtual void DetachDirectorFilter(AgentId director, IDirectorFilter& filter) = 0;

        //! Wakes agents whose running action was waiting to be told something.
        virtual void WakeAgents(AZStd::span<const AgentId> agents) = 0;

        //! What a backend may reach of the core while compiling and deciding.
        //! @{
        //! The authored node tree a program was declared as.
        virtual AZ::Outcome<AZStd::shared_ptr<const AuthoredNode>, AZStd::string> EmitProgram(
            const AZ::Name& name) = 0;
        //! What a named slot currently points at, or an empty name when nothing is bound.
        virtual AZ::Name GetSubtreeBinding(const AZ::Name& slot) const = 0;
        //! The verb a word runs, or Invalid when no module registered one.
        virtual ActionStateId FindVerb(const AZ::Name& name) const = 0;
        //! What a word declares it accepts, or nullptr.
        virtual const NodeTypeDescriptor* FindNodeType(const AZ::Name& name) const = 0;
        //! The backend a delegate named, or nullptr.
        virtual IBackend* FindBackend(const AZ::Name& name) const = 0;
        //! Runs one authored behaviour for one agent.
        virtual ActionResult CallBehavior(
            const AZ::Name& behavior, const char* phase, AgentId agent, float deltaTime) = 0;
        //! True when a behaviour of that name was declared, so a program naming one that is not
        //! there is refused where it was written rather than found missing on the tick that needed it.
        virtual bool HasBehavior(const AZ::Name& behavior) const = 0;
        //! Runs one phase of a behaviour and reads the number it answered with, for a phase that
        //! measures rather than acts. False when nothing answered, which a caller has to tell
        //! apart from an answer of zero: only one of the two is a mistake.
        virtual bool MeasureBehavior(const AZ::Name& behavior, const char* phase, AgentId agent,
            AZStd::span<const float> values, float& outValue) = 0;
        //! @}

        //! Installs a backend. Removing one is what makes backends decoupled.
        virtual bool RegisterBackend(AZStd::unique_ptr<IBackend> backend) = 0;
        virtual void UnregisterBackend(const AZ::Name& name) = 0;

        //! Installs a node type, which is how a module contributes a word to authored trees.
        //! A leaf whose name matches a registered verb runs that verb.
        virtual bool RegisterNodeType(NodeTypeDescriptor descriptor) = 0;
        virtual void UnregisterNodeType(const AZ::Name& name) = 0;

        //! Installs a Lua vocabulary file, run right after the core's own words.
        //! @assetPath the cache path without an extension; the compiled form is preferred.
        virtual void RegisterVocabularyScript(AZStd::string_view assetPath) = 0;
        virtual void UnregisterVocabularyScript(AZStd::string_view assetPath) = 0;

        //! Installs an action verb, which is how a module contributes vocabulary.
        virtual ActionStateId RegisterAction(AZStd::unique_ptr<IActionState> action) = 0;
        virtual void UnregisterAction(ActionStateId id) = 0;

        //! Names of what is currently installed, for console output and validation.
        virtual AZStd::vector<AZ::Name> GetBackendNames() const = 0;
        virtual AZStd::vector<AZ::Name> GetActionNames() const = 0;
        virtual AZStd::vector<AZ::Name> GetTreeNames() const = 0;
        virtual AZStd::vector<AZ::Name> GetNodeTypeNames() const = 0;

        //! A one line summary of what an agent is doing, for the console.
        virtual AZStd::string DescribeAgent(AgentId agent) const = 0;

        //! Everything a tool shows about one agent, in one pass. False when there is no such
        //! agent. The active path is filled in by whatever backend owns the agent's program,
        //! because only that backend can read the state block it wrote.
        virtual bool SnapshotAgent(AgentId agent, AgentSnapshot& outSnapshot) const = 0;

        //! The same for every registered agent, which is what an agent browser polls.
        virtual AZStd::vector<AgentSnapshot> SnapshotAgents() const = 0;
    };

    //! Registered by the GOAT system component for the lifetime of the gem.
    using AgentSystemInterface = AZ::Interface<IAgentSystem>;
} // namespace GOAT
