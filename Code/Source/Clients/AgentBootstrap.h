#pragma once

#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/Assets/ProgramAsset.h>
#include <GOAT/Domain/AgentId.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Script/ScriptAsset.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! What an entity needs authored before it can become an agent.
    //! Shared by the agent and the director components, which differ in what they do afterwards
    //! and not at all in how they get there.
    struct AgentBootstrapRequest final
    {
        AZ::EntityId m_entity;
        AZStd::vector<AZ::Data::Asset<BlackboardAsset>>* m_blackboards = nullptr;
        AZStd::vector<AZ::Data::Asset<AZ::ScriptAsset>>* m_scripts = nullptr;
        //! Graph authored programs, each declaring itself under the name it carries.
        AZStd::vector<AZ::Data::Asset<ProgramAsset>>* m_programAssets = nullptr;
        //! What runs this entity's programs.
        AZStd::string m_brain;
        //! Programs this entity may run. The first is the one it starts in.
        //! A program asset adds its own name here when it is not listed already.
        const AZStd::vector<AZStd::string>* m_programs = nullptr;
        AZStd::string m_squad;
        int m_band = 1;
    };

    //! Declares the variables, runs the scripts, compiles every listed program and registers the
    //! agent. Returns a null handle when anything on that path failed.
    //!
    //! The order is load bearing: variables before programs, because a guard resolves its name to
    //! a slot when the program compiles; and every program rather than only the first, so a name
    //! the entity switches to much later fails now, while whoever wrote it is still looking.
    AgentId BootstrapAgent(const AgentBootstrapRequest& request);
} // namespace GOAT
