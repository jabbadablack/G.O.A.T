#pragma once

#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/Domain/AgentId.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Script/ScriptAsset.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! Turns an entity into an agent running a named tree.
    //! Everything it needs is authored: blackboard assets declare the variables,
    //! Lua scripts declare the behaviours and the tree, and the tree name selects one.
    class GOATAgentComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT_DECL(GOATAgentComponent);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        //! The agent this component registered, or a null handle when it is not running.
        AgentId GetAgentId() const { return m_agent; }

    protected:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

    private:
        //! Blackboard assets whose variables this agent's tree refers to.
        AZStd::vector<AZ::Data::Asset<BlackboardAsset>> m_blackboards;
        //! Lua scripts declaring behaviours, backends and trees.
        AZStd::vector<AZ::Data::Asset<AZ::ScriptAsset>> m_scripts;
        //! Which declared tree this agent runs.
        AZStd::string m_treeName;
        //! Squad this agent joins, if any.
        AZStd::string m_squad;
        //! How often the agent runs, from zero for the most frequent band.
        int m_band = 1;

        AgentId m_agent;
    };
} // namespace GOAT
