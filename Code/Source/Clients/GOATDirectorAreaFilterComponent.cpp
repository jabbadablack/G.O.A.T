#include <Clients/GOATDirectorAreaFilterComponent.h>

#include <Clients/GOATDirectorComponent.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <LmbrCentral/Shape/ShapeComponentBus.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATDirectorAreaFilterComponent, "GOATDirectorAreaFilterComponent",
        GOATDirectorAreaFilterComponentTypeId);

    void GOATDirectorAreaFilterComponent::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Class<GOATDirectorAreaFilterComponent, AZ::Component>()
            ->Version(1);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (editContext == nullptr)
        {
            return;
        }

        editContext->Class<GOATDirectorAreaFilterComponent>("GOAT Director Area",
            "Governs only the agents inside this entity's shape")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Category, "AI")
            ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/GOAT/Components/GOAT.svg")
            ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/GOAT/Components/Viewport/GOAT.svg");
    }

    void GOATDirectorAreaFilterComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOATDirectorAreaFilterService"));
    }

    void GOATDirectorAreaFilterComponent::GetIncompatibleServices(
        AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOATDirectorAreaFilterService"));
    }

    void GOATDirectorAreaFilterComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // The director activates first, so its handle is there to attach to; the shape is the area
        // itself, and requiring it is what tells an author the component needs one.
        required.push_back(AZ_CRC_CE("GOATDirectorService"));
        required.push_back(AZ_CRC_CE("ShapeService"));
    }

    void GOATDirectorAreaFilterComponent::SetShapeEntity(AZ::EntityId shape)
    {
        m_shape = shape.IsValid() ? shape : GetEntityId();
        m_reportedMissingShape = false;
    }

    bool GOATDirectorAreaFilterComponent::Accepts([[maybe_unused]] AgentId agent, AZ::EntityId entity) const
    {
        // Fail open. A filter that cannot answer must not quietly change who is governed, and a
        // director that suddenly governs nobody is far harder to diagnose than one warning.
        if (!LmbrCentral::ShapeComponentRequestsBus::HasHandlers(m_shape))
        {
            AZ_Warning("GOAT", m_reportedMissingShape,
                "Entity %s has no shape to filter a director's reach by, so it governs everyone",
                m_shape.ToString().c_str());
            m_reportedMissingShape = true;
            return true;
        }

        if (!AZ::TransformBus::HasHandlers(entity))
        {
            return true;
        }

        AZ::Vector3 position = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(position, entity, &AZ::TransformInterface::GetWorldTranslation);

        bool inside = true;
        LmbrCentral::ShapeComponentRequestsBus::EventResult(
            inside, m_shape, &LmbrCentral::ShapeComponentRequests::IsPointInside, position);
        return inside;
    }

    void GOATDirectorAreaFilterComponent::Activate()
    {
        m_shape = GetEntityId();

        auto* director = GetEntity()->FindComponent<GOATDirectorComponent>();
        m_director = director != nullptr ? director->GetAgentId() : AgentId{};
        if (m_director.IsNull())
        {
            AZ_Error("GOAT", false, "Entity %s has no running director for its area filter to narrow",
                GetEntityId().ToString().c_str());
            return;
        }

        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr || !agents->AttachDirectorFilter(m_director, *this))
        {
            m_director = AgentId{};
            return;
        }

        GOATDirectorAreaFilterRequestBus::Handler::BusConnect(GetEntityId());
        AZLOG_INFO("GOAT: director on entity %s governs only what its shape contains",
            GetEntityId().ToString().c_str());
    }

    void GOATDirectorAreaFilterComponent::Deactivate()
    {
        GOATDirectorAreaFilterRequestBus::Handler::BusDisconnect();

        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents != nullptr && !m_director.IsNull())
        {
            agents->DetachDirectorFilter(m_director, *this);
        }
        m_director = AgentId{};
    }
} // namespace GOAT
