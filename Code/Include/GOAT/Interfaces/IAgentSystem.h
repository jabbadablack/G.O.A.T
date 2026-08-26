#pragma once

#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/NodeType.h>
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
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>

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

        //! Compiles a declared tree so agents can run it. Always recompiles, which is what
        //! makes it the way to pick up a rebound dynamic subtree.
        virtual AZ::Outcome<void, AZStd::string> CompileTree(const AZ::Name& treeName) = 0;

        //! True when a tree is already compiled and agents can be registered against it.
        //! Tree names share one namespace, like blackboard variable names.
        virtual bool IsTreeCompiled(const AZ::Name& treeName) const = 0;

        //! Registers an entity as an agent running a compiled tree.
        //! The band selects how often it runs, from most frequent at zero.
        virtual AgentId RegisterAgent(AZ::EntityId entity, const AZ::Name& treeName, size_t band) = 0;

        //! Removes an agent and everything held for it.
        virtual void UnregisterAgent(AgentId agent) = 0;

        //! Puts an agent in a named squad, creating that squad on the first join.
        virtual void JoinSquad(AgentId agent, const AZ::Name& squad) = 0;

        //! Installs a backend. Removing one is what makes backends decoupled.
        virtual bool RegisterBackend(AZStd::unique_ptr<IBackend> backend) = 0;
        virtual void UnregisterBackend(const AZ::Name& name) = 0;

        //! Installs a node type, which is how a module contributes a word to authored trees.
        //! A leaf whose name matches a registered verb runs that verb.
        virtual bool RegisterNodeType(NodeTypeDescriptor descriptor) = 0;
        virtual void UnregisterNodeType(const AZ::Name& name) = 0;

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
    };

    //! Registered by the GOAT system component for the lifetime of the gem.
    using AgentSystemInterface = AZ::Interface<IAgentSystem>;
} // namespace GOAT
