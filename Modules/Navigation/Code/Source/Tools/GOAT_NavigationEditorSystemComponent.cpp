
#include <AzCore/Serialization/SerializeContext.h>
#include "GOAT_NavigationEditorSystemComponent.h"

#include <GOAT_Navigation/GOAT_NavigationTypeIds.h>

namespace GOAT_Navigation
{
    AZ_COMPONENT_IMPL(GOAT_NavigationEditorSystemComponent, "GOAT_NavigationEditorSystemComponent",
        GOAT_NavigationEditorSystemComponentTypeId, BaseSystemComponent);

    void GOAT_NavigationEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_NavigationEditorSystemComponent, GOAT_NavigationSystemComponent>()
                ->Version(0);
        }
    }

    GOAT_NavigationEditorSystemComponent::GOAT_NavigationEditorSystemComponent() = default;

    GOAT_NavigationEditorSystemComponent::~GOAT_NavigationEditorSystemComponent() = default;

    void GOAT_NavigationEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("GOAT_NavigationEditorService"));
    }

    void GOAT_NavigationEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("GOAT_NavigationEditorService"));
    }

    void GOAT_NavigationEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void GOAT_NavigationEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

    void GOAT_NavigationEditorSystemComponent::Activate()
    {
        GOAT_NavigationSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void GOAT_NavigationEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        GOAT_NavigationSystemComponent::Deactivate();
    }

} // namespace GOAT_Navigation
