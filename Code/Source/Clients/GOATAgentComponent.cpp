#include <Clients/GOATAgentComponent.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATAgentComponent, "GOATAgentComponent", GOATAgentComponentTypeId);

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

    void GOATAgentComponent::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Class<GOATAgentComponent, AZ::Component>()
            ->Version(2)
            ->Field("Blackboards", &GOATAgentComponent::m_blackboards)
            ->Field("Scripts", &GOATAgentComponent::m_scripts)
            ->Field("Trees", &GOATAgentComponent::m_trees)
            // Kept so a level saved before an agent could hold several trees still loads.
            ->Field("TreeName", &GOATAgentComponent::m_legacyTreeName)
            ->Field("Squad", &GOATAgentComponent::m_squad)
            ->Field("Band", &GOATAgentComponent::m_band);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (editContext == nullptr)
        {
            return;
        }

        editContext->Class<GOATAgentComponent>("GOAT Agent", "Runs a behavior tree on this entity")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Category, "AI")
            ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/GOAT/Components/GOATAgent.svg")
            ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/GOAT/Components/Viewport/GOATAgent.svg")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATAgentComponent::m_blackboards, "Blackboards",
                "Blackboard assets declaring the variables this tree uses")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATAgentComponent::m_scripts, "Scripts",
                "Lua scripts declaring behaviours, backends and trees")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATAgentComponent::m_trees, "Trees",
                "Declared trees this agent may run. The first is the one it starts in, and every "
                "one of them is compiled when this entity activates.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATAgentComponent::m_squad, "Squad",
                "Squad this agent joins, leave empty for none")
            ->DataElement(
                AZ::Edit::UIHandlers::Slider, &GOATAgentComponent::m_band, "Detail",
                "How often this agent runs, zero being the most frequent")
                ->Attribute(AZ::Edit::Attributes::Min, 0)
                ->Attribute(AZ::Edit::Attributes::Max, 3);
    }

    void GOATAgentComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOATAgentService"));
    }

    void GOATAgentComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOATAgentService"));
    }

    void GOATAgentComponent::Activate()
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            AZ_Warning("GOAT", false, "The GOAT agent system is not available");
            return;
        }

        // Variables must be declared before the tree compiles, because a guard resolves
        // its blackboard name to a slot at compile time.
        for (auto& blackboard : m_blackboards)
        {
            if (EnsureLoaded(blackboard))
            {
                if (auto declared = agents->LoadBlackboard(*blackboard.Get()); !declared.IsSuccess())
                {
                    AZ_Warning("GOAT", false, "%s", declared.GetError().c_str());
                }
            }
        }

        for (auto& script : m_scripts)
        {
            if (EnsureLoaded(script))
            {
                agents->LoadScript(script);
            }
        }

        // A level saved before an agent could hold several trees names one; fold it in so those
        // levels keep working without anyone having to re-author them.
        if (m_trees.empty() && !m_legacyTreeName.empty())
        {
            m_trees.push_back(m_legacyTreeName);
        }

        if (m_trees.empty())
        {
            AZ_Warning("GOAT", false, "Entity %s names no tree to run", GetEntityId().ToString().c_str());
            return;
        }

        // Compile all of them, not just the one it starts in. A tree this agent only switches to
        // much later is worth failing on now, while whoever named it is still looking at it.
        for (const AZStd::string& name : m_trees)
        {
            if (name.empty())
            {
                AZ_Warning("GOAT", false, "Entity %s lists an unnamed tree", GetEntityId().ToString().c_str());
                continue;
            }

            // The compiled program is immutable and shared, so agents past the first reuse it
            // rather than re-emitting and recompiling identical content once per entity.
            const AZ::Name candidate(name);
            if (agents->IsTreeCompiled(candidate))
            {
                continue;
            }

            if (auto compiled = agents->CompileTree(candidate); !compiled.IsSuccess())
            {
                AZ_Warning("GOAT", false, "%s", compiled.GetError().c_str());
            }
        }

        const AZ::Name treeName(m_trees.front());
        if (!agents->IsTreeCompiled(treeName))
        {
            AZ_Error("GOAT", false, "Entity %s cannot start: its first tree '%s' did not compile",
                GetEntityId().ToString().c_str(), treeName.GetCStr());
            return;
        }

        m_agent = agents->RegisterAgent(GetEntityId(), treeName, static_cast<size_t>(m_band));

        if (!m_squad.empty() && !m_agent.IsNull())
        {
            agents->JoinSquad(m_agent, AZ::Name(m_squad));
        }
    }

    void GOATAgentComponent::Deactivate()
    {
        if (IAgentSystem* agents = AgentSystemInterface::Get(); agents != nullptr && !m_agent.IsNull())
        {
            agents->UnregisterAgent(m_agent);
        }
        m_agent = AgentId{};
    }
} // namespace GOAT
