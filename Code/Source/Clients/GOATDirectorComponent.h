#pragma once

#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/Assets/ProgramAsset.h>
#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/DirectorProfile.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Script/ScriptAsset.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! An authority that observes the other agents and reshapes what they are doing.
    //!
    //! A director is an agent: it registers with the agent system and runs an ordinary behaviour
    //! tree, so guards, services, parallel, plans and the whole console apply to it unchanged.
    //! The only difference is that its leaves act on the agents it governs rather than on itself.
    //!
    //! It governs every other agent until a filter component beside it narrows that. Several
    //! directors may govern the same agent, and priority settles who wins.
    class GOATDirectorComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT_DECL(GOATDirectorComponent);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        //! The agent this component registered, or a null handle when it is not running.
        AgentId GetAgentId() const { return m_agent; }

    protected:
        void Activate() override;
        void Deactivate() override;

    private:
        //! Blackboard assets declaring the variables this director's tree refers to.
        AZStd::vector<AZ::Data::Asset<BlackboardAsset>> m_blackboards;
        //! Lua scripts declaring the behaviours and trees it runs.
        AZStd::vector<AZ::Data::Asset<AZ::ScriptAsset>> m_scripts;
        //! Graph authored programs, each declaring itself under the name it carries.
        AZStd::vector<AZ::Data::Asset<ProgramAsset>> m_programAssets;
        //! What decides how it acts.
        AZStd::string m_brain = "tree";
        //! Programs it may run. The first is the one it starts in.
        AZStd::vector<AZStd::string> m_programs;

        //! Higher outranks lower when two directors command the same agent.
        int m_priority = 1;
        //! How long before it may command the same agent the same way again.
        float m_cooldownSeconds = 5.0f;

        //! How often it runs. Three by default, which is once a second: a director is strategic,
        //! and every shipped one in the literature runs at a fraction of the rate its agents do.
        int m_band = 3;

        AgentId m_agent;
    };
} // namespace GOAT
