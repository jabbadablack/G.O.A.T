#include <Clients/GOATDirectorComponent.h>

#include <Clients/AgentBootstrap.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATDirectorComponent, "GOATDirectorComponent", GOATDirectorComponentTypeId);

    void GOATDirectorComponent::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Class<GOATDirectorComponent, AZ::Component>()
            ->Version(2)
            ->Field("Blackboards", &GOATDirectorComponent::m_blackboards)
            ->Field("Scripts", &GOATDirectorComponent::m_scripts)
            ->Field("Brain", &GOATDirectorComponent::m_brain)
            ->Field("Programs", &GOATDirectorComponent::m_programs)
            ->Field("Squad", &GOATDirectorComponent::m_squad)
            ->Field("Tree", &GOATDirectorComponent::m_tree)
            ->Field("Radius", &GOATDirectorComponent::m_radius)
            ->Field("Filter", &GOATDirectorComponent::m_filter)
            ->Field("Priority", &GOATDirectorComponent::m_priority)
            ->Field("Cooldown", &GOATDirectorComponent::m_cooldownSeconds)
            ->Field("Band", &GOATDirectorComponent::m_band);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (editContext == nullptr)
        {
            return;
        }

        editContext->Class<GOATDirectorComponent>("GOAT Director", "Reshapes what the other agents are doing")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Category, "AI")
            ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/GOAT/Components/GOATAgent.svg")
            ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/GOAT/Components/Viewport/GOATAgent.svg")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_blackboards, "Blackboards",
                "Blackboard assets declaring the variables this director's tree uses")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_scripts, "Scripts",
                "Lua scripts declaring the behaviours and trees it runs")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_brain, "Brain",
                "The backend that decides how this director acts")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_programs, "Programs",
                "Programs this director may run. The first is the one it starts in.")
            ->ClassElement(AZ::Edit::ClassElements::Group, "Governs")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_squad, "Squad",
                "Governs only this squad. Leave empty for any.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_tree, "Tree",
                "Governs only agents currently running this tree. Leave empty for any.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_radius, "Radius",
                "Governs only agents this close. Zero for any distance.")
                ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_filter, "Filter",
                "A reach filter a module contributed, such as path_distance from the navigation "
                "gem. Leave empty for plain straight line distance.")
            ->DataElement(
                AZ::Edit::UIHandlers::SpinBox, &GOATDirectorComponent::m_priority, "Priority",
                "Higher outranks lower when two directors command the same agent. Zero is what an "
                "agent switching its own tree carries, so leave this above zero.")
                ->Attribute(AZ::Edit::Attributes::Min, 0)
                ->Attribute(AZ::Edit::Attributes::Max, 255)
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_cooldownSeconds, "Cooldown",
                "How long before this director may command the same agent the same way again. "
                "Switching a tree stops whatever the agent was doing, so ordering one every tick "
                "would leave it permanently restarting.")
                ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(
                AZ::Edit::UIHandlers::Slider, &GOATDirectorComponent::m_band, "Detail",
                "How often this director runs. Three, once a second, suits a director: it decides "
                "strategy, and reconsidering it every frame would only make it thrash.")
                ->Attribute(AZ::Edit::Attributes::Min, 0)
                ->Attribute(AZ::Edit::Attributes::Max, 3);
    }

    void GOATDirectorComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOATDirectorService"));

        // Also the agent service, because a director registers itself as an agent. Providing it
        // is what stops this sitting beside a GOAT Agent, which declares itself incompatible
        // with the same service: the entity would otherwise be registered twice.
        provided.push_back(AZ_CRC_CE("GOATAgentService"));
    }

    void GOATDirectorComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOATDirectorService"));
        incompatible.push_back(AZ_CRC_CE("GOATAgentService"));
    }

    DirectorProfile GOATDirectorComponent::BuildProfile() const
    {
        DirectorProfile profile;
        profile.m_reach.m_squad = AZ::Name(m_squad);
        profile.m_reach.m_tree = AZ::Name(m_tree);
        profile.m_reach.m_radius = m_radius;
        profile.m_reach.m_filter = AZ::Name(m_filter);
        profile.m_priority = static_cast<AZ::u8>(AZStd::clamp(m_priority, 0, 255));
        profile.m_cooldownSeconds = m_cooldownSeconds;
        return profile;
    }

    void GOATDirectorComponent::Activate()
    {
        AgentBootstrapRequest request;
        request.m_entity = GetEntityId();
        request.m_blackboards = &m_blackboards;
        request.m_scripts = &m_scripts;
        request.m_brain = m_brain;
        request.m_programs = &m_programs;
        request.m_band = m_band;

        m_agent = BootstrapAgent(request);
        if (m_agent.IsNull())
        {
            return;
        }

        IAgentSystem* agents = AgentSystemInterface::Get();
        AZ_Assert(agents != nullptr, "Bootstrapping succeeded, so the agent system is running");
        if (agents == nullptr || !agents->RegisterDirector(m_agent, BuildProfile()))
        {
            return;
        }

        AZLOG_INFO("GOAT: entity %s is directing at priority %d", GetEntityId().ToString().c_str(), m_priority);
    }

    void GOATDirectorComponent::Deactivate()
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr || m_agent.IsNull())
        {
            return;
        }

        // The director record first: it is keyed by the agent handle the line below releases.
        agents->UnregisterDirector(m_agent);
        agents->UnregisterAgent(m_agent);
        m_agent = AgentId{};
    }
} // namespace GOAT
