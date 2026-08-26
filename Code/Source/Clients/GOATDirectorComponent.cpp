#include <Clients/GOATDirectorComponent.h>

#include <Clients/AgentBootstrap.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATDirectorComponent, "GOATDirectorComponent", GOATDirectorComponentTypeId);

    void GOATDirectorComponent::Reflect(AZ::ReflectContext* context)
    {
        DirectorReach::Reflect(context);
        DirectorProfile::Reflect(context);

        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Class<GOATDirectorComponent, AZ::Component>()
            ->Version(1)
            ->Field("Blackboards", &GOATDirectorComponent::m_blackboards)
            ->Field("Scripts", &GOATDirectorComponent::m_scripts)
            ->Field("Trees", &GOATDirectorComponent::m_trees)
            ->Field("Profile", &GOATDirectorComponent::m_profile)
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
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_trees, "Trees",
                "Trees this director may run. The first is the one it starts in.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorComponent::m_profile, "Governs",
                "Which agents it commands, and how forcefully")
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

    void GOATDirectorComponent::Activate()
    {
        AgentBootstrapRequest request;
        request.m_entity = GetEntityId();
        request.m_blackboards = &m_blackboards;
        request.m_scripts = &m_scripts;
        request.m_trees = &m_trees;
        request.m_band = m_band;

        m_agent = BootstrapAgent(request);
        if (m_agent.IsNull())
        {
            return;
        }

        IAgentSystem* agents = AgentSystemInterface::Get();
        AZ_Assert(agents != nullptr, "Bootstrapping succeeded, so the agent system is running");
        if (agents == nullptr || !agents->RegisterDirector(m_agent, m_profile))
        {
            return;
        }

        AZLOG_INFO("GOAT: entity %s is directing at priority %u", GetEntityId().ToString().c_str(),
            static_cast<AZ::u32>(m_profile.m_priority));
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
