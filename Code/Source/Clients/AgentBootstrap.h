#pragma once

#include <GOAT/Assets/BlackboardAsset.h>
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
        //! Trees this entity may run. The first is the one it starts in.
        const AZStd::vector<AZStd::string>* m_trees = nullptr;
        AZStd::string m_squad;
        int m_band = 1;
    };

    //! Declares the variables, runs the scripts, compiles every listed tree and registers the
    //! agent. Returns a null handle when anything on that path failed.
    //!
    //! The order is load bearing: variables before trees, because a guard resolves its name to a
    //! slot when the tree compiles; and every tree rather than only the first, so a name the
    //! entity switches to much later fails now, while whoever wrote it is still looking at it.
    AgentId BootstrapAgent(const AgentBootstrapRequest& request);
} // namespace GOAT
