#include <Clients/AgentBootstrap.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    namespace
    {
        //! Blocks until an asset is usable, so activation order does not decide whether
        //! an agent's variables were declared before its tree compiled.
        template<typename AssetType>
        bool EnsureLoaded(AZ::Data::Asset<AssetType>& asset)
        {
            if (!asset.GetId().IsValid())
            {
                return false;
            }
            if (!asset.IsReady())
            {
                asset.QueueLoad();
                asset.BlockUntilLoadComplete();
            }
            return asset.IsReady();
        }
    } // namespace

    AgentId BootstrapAgent(const AgentBootstrapRequest& request)
    {
        AZ_Assert(request.m_trees != nullptr, "An entity always says which trees it may run");

        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            AZ_Warning("GOAT", false, "The GOAT agent system is not available");
            return AgentId{};
        }

        // Variables first: a guard resolves its blackboard name to a slot when the tree compiles,
        // so a tree compiled before its variables exist can never guard on them.
        if (request.m_blackboards != nullptr)
        {
            for (auto& blackboard : *request.m_blackboards)
            {
                if (EnsureLoaded(blackboard))
                {
                    if (auto declared = agents->LoadBlackboard(*blackboard.Get()); !declared.IsSuccess())
                    {
                        AZ_Warning("GOAT", false, "%s", declared.GetError().c_str());
                    }
                }
            }
        }

        if (request.m_scripts != nullptr)
        {
            for (auto& script : *request.m_scripts)
            {
                if (EnsureLoaded(script))
                {
                    agents->LoadScript(script);
                }
            }
        }

        if (request.m_trees->empty())
        {
            AZ_Warning("GOAT", false, "Entity %s names no tree to run", request.m_entity.ToString().c_str());
            return AgentId{};
        }

        // All of them, not just the one it starts in. A tree this entity only switches to much
        // later is worth failing on now, while whoever named it is still looking at it.
        // The same list becomes the agent's repertoire, so what it declares is what it may run.
        AZStd::vector<AZ::Name> repertoire;
        repertoire.reserve(request.m_trees->size());

        for (const AZStd::string& name : *request.m_trees)
        {
            if (name.empty())
            {
                AZ_Warning("GOAT", false, "Entity %s lists an unnamed tree", request.m_entity.ToString().c_str());
                continue;
            }

            // Listed whether or not it compiles: the repertoire is what the author declared, so a
            // tree that failed to compile is reported as that rather than as one never declared.
            const AZ::Name candidate(name);
            repertoire.push_back(candidate);

            // The compiled program is immutable and shared, so entities past the first reuse it
            // rather than re-emitting and recompiling identical content once each.
            if (agents->IsTreeCompiled(candidate))
            {
                continue;
            }

            if (auto compiled = agents->CompileTree(candidate); !compiled.IsSuccess())
            {
                AZ_Warning("GOAT", false, "%s", compiled.GetError().c_str());
            }
        }

        const AZ::Name treeName(request.m_trees->front());
        if (!agents->IsTreeCompiled(treeName))
        {
            AZ_Error("GOAT", false, "Entity %s cannot start: its first tree '%s' did not compile",
                request.m_entity.ToString().c_str(), treeName.GetCStr());
            return AgentId{};
        }

        // The squad goes in with the registration rather than after it, so squad scoped guards
        // are armed against storage that already exists.
        return agents->RegisterAgent(
            request.m_entity, treeName, static_cast<size_t>(request.m_band), AZ::Name(request.m_squad), repertoire);
    }
} // namespace GOAT
