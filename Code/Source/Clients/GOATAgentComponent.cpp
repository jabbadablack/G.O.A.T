#include <Clients/GOATAgentComponent.h>
#include <Clients/AgentBootstrap.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATAgentComponent, "GOATAgentComponent", GOATAgentComponentTypeId);

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
        // A level saved before an agent could hold several trees names one; fold it in so those
        // levels keep working without anyone having to re-author them.
        if (m_trees.empty() && !m_legacyTreeName.empty())
        {
            m_trees.push_back(m_legacyTreeName);
        }

        AgentBootstrapRequest request;
        request.m_entity = GetEntityId();
        request.m_blackboards = &m_blackboards;
        request.m_scripts = &m_scripts;
        request.m_trees = &m_trees;
        request.m_squad = m_squad;
        request.m_band = m_band;

        m_agent = BootstrapAgent(request);
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
