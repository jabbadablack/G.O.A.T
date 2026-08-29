#include <Clients/AgentBootstrap.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/algorithm.h>
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
        AZ_Assert(request.m_programs != nullptr, "An entity always says which programs it may run");

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

        // Names an asset declared, kept in the order the assets were listed.
        AZStd::vector<AZStd::string> fromAssets;
        if (request.m_programAssets != nullptr)
        {
            for (auto& program : *request.m_programAssets)
            {
                if (!EnsureLoaded(program))
                {
                    continue;
                }
                if (auto loaded = agents->LoadProgram(*program.Get()); !loaded.IsSuccess())
                {
                    AZ_Warning("GOAT", false, "%s", loaded.GetError().c_str());
                    continue;
                }
                fromAssets.push_back(program.Get()->m_name);
            }
        }

        // What the author listed, then whatever an asset brought that they did not list, so
        // dropping a .goat on the entity is enough on its own.
        AZStd::vector<AZStd::string> names = *request.m_programs;
        for (const AZStd::string& name : fromAssets)
        {
            if (AZStd::find(names.begin(), names.end(), name) == names.end())
            {
                names.push_back(name);
            }
        }

        if (names.empty())
        {
            AZ_Warning("GOAT", false, "Entity %s names no program to run", request.m_entity.ToString().c_str());
            return AgentId{};
        }

        // What runs it, defaulted so an entity that says nothing still gets a behaviour tree.
        const AZ::Name backend(request.m_brain.empty() ? "tree" : request.m_brain.c_str());

        // All of them, not just the one it starts in. A program this entity only switches to much
        // later is worth failing on now, while whoever named it is still looking at it.
        AZStd::vector<AZ::Name> declared;
        declared.reserve(names.size());

        for (const AZStd::string& name : names)
        {
            if (name.empty())
            {
                AZ_Warning("GOAT", false, "Entity %s lists an unnamed program", request.m_entity.ToString().c_str());
                continue;
            }

            // Listed whether or not it compiles: what an agent may run is what the author
            // declared, so one that failed to compile is reported as that rather than as absent.
            const AZ::Name candidate(name);
            declared.push_back(candidate);

            // The compiled program is immutable and shared, so entities past the first reuse it
            // rather than re-emitting and recompiling identical content once each.
            if (agents->IsProgramCompiled(candidate))
            {
                continue;
            }

            if (auto compiled = agents->CompileProgram(backend, candidate); !compiled.IsSuccess())
            {
                AZ_Warning("GOAT", false, "%s", compiled.GetError().c_str());
            }
        }

        if (declared.empty() || !agents->IsProgramCompiled(declared.front()))
        {
            AZ_Error("GOAT", false, "Entity %s cannot start: its first program did not compile",
                request.m_entity.ToString().c_str());
            return AgentId{};
        }

        // The squad goes in with the registration rather than after it, so squad scoped guards
        // are armed against storage that already exists.
        return agents->RegisterAgent(
            request.m_entity, backend, declared, static_cast<size_t>(request.m_band), AZ::Name(request.m_squad));
    }
} // namespace GOAT
