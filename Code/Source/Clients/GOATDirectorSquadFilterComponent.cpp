#include <Clients/GOATDirectorSquadFilterComponent.h>

#include <Clients/GOATDirectorComponent.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/algorithm.h>

#include <LmbrCentral/Scripting/TagComponentBus.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATDirectorSquadFilterComponent, "GOATDirectorSquadFilterComponent",
        GOATDirectorSquadFilterComponentTypeId);

    void GOATDirectorSquadFilterComponent::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Class<GOATDirectorSquadFilterComponent, AZ::Component>()
            ->Version(1)
            ->Field("Squads", &GOATDirectorSquadFilterComponent::m_squads)
            ->Field("Tags", &GOATDirectorSquadFilterComponent::m_tags);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (editContext == nullptr)
        {
            return;
        }

        editContext->Class<GOATDirectorSquadFilterComponent>("GOAT Director Filter",
            "Governs only the agents in these squads or carrying these tags")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Category, "AI")
            ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/GOAT/Components/GOAT.svg")
            ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/GOAT/Components/Viewport/GOAT.svg")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorSquadFilterComponent::m_squads, "Squads",
                "Squads this director governs. Leave empty to govern none by squad.")
            ->DataElement(
                AZ::Edit::UIHandlers::Default, &GOATDirectorSquadFilterComponent::m_tags, "Tags",
                "Tags this director governs, read from the entity's Tag component. An agent is "
                "governed if its squad is listed or any of its tags is.");
    }

    void GOATDirectorSquadFilterComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOATDirectorSquadFilterService"));
    }

    void GOATDirectorSquadFilterComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOATDirectorSquadFilterService"));
    }

    void GOATDirectorSquadFilterComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("GOATDirectorService"));
    }

    bool GOATDirectorSquadFilterComponent::Accepts(AgentId agent, AZ::EntityId entity) const
    {
        // Nothing authored is no constraint, which keeps an empty component from silently
        // stripping a director of everyone it governs.
        if (m_squadNames.empty() && m_tagIds.empty())
        {
            return true;
        }

        if (!m_squadNames.empty())
        {
            IAgentSystem* agents = AgentSystemInterface::Get();
            const AZ::Name squad = agents != nullptr ? agents->GetAgentSquad(agent) : AZ::Name();
            if (AZStd::find(m_squadNames.begin(), m_squadNames.end(), squad) != m_squadNames.end())
            {
                return true;
            }
        }

        for (const AZ::Crc32 tag : m_tagIds)
        {
            bool tagged = false;
            LmbrCentral::TagComponentRequestBus::EventResult(
                tagged, entity, &LmbrCentral::TagComponentRequests::HasTag, tag);
            if (tagged)
            {
                return true;
            }
        }

        return false;
    }

    void GOATDirectorSquadFilterComponent::Activate()
    {
        // Interned once: this is compared against every agent in the level on every director tick.
        m_squadNames.clear();
        m_squadNames.reserve(m_squads.size());
        for (const AZStd::string& squad : m_squads)
        {
            if (!squad.empty())
            {
                m_squadNames.emplace_back(squad);
            }
        }

        m_tagIds.clear();
        m_tagIds.reserve(m_tags.size());
        for (const AZStd::string& tag : m_tags)
        {
            if (!tag.empty())
            {
                m_tagIds.emplace_back(tag.c_str());
            }
        }

        AZ_Warning("GOAT", !m_squadNames.empty() || !m_tagIds.empty(),
            "Entity %s filters a director by squad and tag but names neither, so it narrows nothing",
            GetEntityId().ToString().c_str());

        auto* director = GetEntity()->FindComponent<GOATDirectorComponent>();
        m_director = director != nullptr ? director->GetAgentId() : AgentId{};
        if (m_director.IsNull())
        {
            AZ_Error("GOAT", false, "Entity %s has no running director for its squad filter to narrow",
                GetEntityId().ToString().c_str());
            return;
        }

        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr || !agents->AttachDirectorFilter(m_director, *this))
        {
            m_director = AgentId{};
            return;
        }

        AZLOG_INFO("GOAT: director on entity %s governs %zu squad(s) and %zu tag(s)",
            GetEntityId().ToString().c_str(), m_squadNames.size(), m_tagIds.size());
    }

    void GOATDirectorSquadFilterComponent::Deactivate()
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents != nullptr && !m_director.IsNull())
        {
            agents->DetachDirectorFilter(m_director, *this);
        }
        m_director = AgentId{};
    }
} // namespace GOAT
