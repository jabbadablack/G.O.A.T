
#include <AzCore/Serialization/SerializeContext.h>
#include "GOAT_SmartObjectEditorSystemComponent.h"

#include <GOAT_SmartObject/GOAT_SmartObjectTypeIds.h>

namespace GOAT_SmartObject
{
    AZ_COMPONENT_IMPL(GOAT_SmartObjectEditorSystemComponent, "GOAT_SmartObjectEditorSystemComponent",
        GOAT_SmartObjectEditorSystemComponentTypeId, BaseSystemComponent);

    void GOAT_SmartObjectEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_SmartObjectEditorSystemComponent, GOAT_SmartObjectSystemComponent>()
                ->Version(0);
        }
    }

    GOAT_SmartObjectEditorSystemComponent::GOAT_SmartObjectEditorSystemComponent() = default;

    GOAT_SmartObjectEditorSystemComponent::~GOAT_SmartObjectEditorSystemComponent() = default;

    void GOAT_SmartObjectEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("GOAT_SmartObjectEditorService"));
    }

    void GOAT_SmartObjectEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("GOAT_SmartObjectEditorService"));
    }

    void GOAT_SmartObjectEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void GOAT_SmartObjectEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

    void GOAT_SmartObjectEditorSystemComponent::Activate()
    {
        GOAT_SmartObjectSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void GOAT_SmartObjectEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        GOAT_SmartObjectSystemComponent::Deactivate();
    }

} // namespace GOAT_SmartObject
