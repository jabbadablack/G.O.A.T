#include <Components/GOATSmartObjectComponent.h>

#include <GOAT_SmartObject/GOAT_SmartObjectBus.h>
#include <GOAT_SmartObject/GOAT_SmartObjectTypeIds.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT_SmartObject
{
    AZ_COMPONENT_IMPL(GOATSmartObjectComponent, "GOATSmartObjectComponent", GOATSmartObjectComponentTypeId);

    void GOATSmartObjectComponent::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Class<GOATSmartObjectComponent, AZ::Component>()
            ->Version(1)
            ->Field("Uses", &GOATSmartObjectComponent::m_uses)
            ->Field("AnchorOffset", &GOATSmartObjectComponent::m_anchorOffset)
            ->Field("Capacity", &GOATSmartObjectComponent::m_capacity)
            ;

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (editContext == nullptr)
        {
            return;
        }

        editContext->Class<GOATSmartObjectComponent>("GOAT Smart Object", "Offers this entity to GOAT agents")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Category, "AI")
            ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC_CE("Game"))
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/GOAT/Components/GOAT.svg")
            ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/GOAT/Components/Viewport/GOAT.svg")
            ->DataElement(AZ::Edit::UIHandlers::Default, &GOATSmartObjectComponent::m_uses, "Uses",
                "What an agent asks for, as in \"sit\". A tree claims one of these by name.")
            ->DataElement(AZ::Edit::UIHandlers::Default, &GOATSmartObjectComponent::m_anchorOffset, "Anchor offset",
                "Where the agent should stand, relative to this entity. Published as so_anchor.")
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &GOATSmartObjectComponent::m_capacity, "Capacity",
                "How many agents may use this at once.")
                ->Attribute(AZ::Edit::Attributes::Min, 1)
            ;
    }

    void GOATSmartObjectComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOATSmartObjectService"));
    }

    void GOATSmartObjectComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOATSmartObjectService"));
    }

    void GOATSmartObjectComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        // The anchor is this entity's transform plus an offset, so there has to be one.
        required.push_back(AZ_CRC_CE("TransformService"));
    }

    void GOATSmartObjectComponent::Activate()
    {
        AZ_Assert(GetEntityId().IsValid(), "A component only activates on a valid entity");

        if (GOAT_SmartObjectInterface::Get() == nullptr)
        {
            AZ_Error("GOAT", false, "The GOAT smart object system is not running, so entity %s offers nothing",
                GetEntityId().ToString().c_str());
            return;
        }

        SmartObjectDescription description;
        description.m_uses.reserve(m_uses.size());
        for (const AZStd::string& use : m_uses)
        {
            AZ_Warning("GOAT", !use.empty(), "Smart object %s lists an unnamed use, which nothing can claim",
                GetEntityId().ToString().c_str());
            if (!use.empty())
            {
                description.m_uses.emplace_back(use);
            }
        }

        description.m_anchorOffset = m_anchorOffset;
        description.m_capacity = m_capacity;

        AZLOG_INFO("GOAT: entity %s offers %zu smart object use(s)",
            GetEntityId().ToString().c_str(), description.m_uses.size());

        GOAT_SmartObjectRequestBus::Broadcast(
            &GOAT_SmartObjectRequests::RegisterObject, GetEntityId(), AZStd::move(description));
    }

    void GOATSmartObjectComponent::Deactivate()
    {
        if (GOAT_SmartObjectInterface::Get() == nullptr)
        {
            return;
        }

        GOAT_SmartObjectRequestBus::Broadcast(&GOAT_SmartObjectRequests::UnregisterObject, GetEntityId());
    }
} // namespace GOAT_SmartObject
