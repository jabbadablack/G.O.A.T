
#include <AzCore/Serialization/SerializeContext.h>
#include "GOAT_UtilityEditorSystemComponent.h"

#include <GOAT_Utility/GOAT_UtilityTypeIds.h>

namespace GOAT_Utility
{
    AZ_COMPONENT_IMPL(GOAT_UtilityEditorSystemComponent, "GOAT_UtilityEditorSystemComponent",
        GOAT_UtilityEditorSystemComponentTypeId, BaseSystemComponent);

    void GOAT_UtilityEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_UtilityEditorSystemComponent, GOAT_UtilitySystemComponent>()
                ->Version(0);
        }
    }

    GOAT_UtilityEditorSystemComponent::GOAT_UtilityEditorSystemComponent() = default;

    GOAT_UtilityEditorSystemComponent::~GOAT_UtilityEditorSystemComponent() = default;

    void GOAT_UtilityEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("GOAT_UtilityEditorService"));
    }

    void GOAT_UtilityEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("GOAT_UtilityEditorService"));
    }

    void GOAT_UtilityEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void GOAT_UtilityEditorSystemComponent::Activate()
    {
        GOAT_UtilitySystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void GOAT_UtilityEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        GOAT_UtilitySystemComponent::Deactivate();
    }

} // namespace GOAT_Utility
