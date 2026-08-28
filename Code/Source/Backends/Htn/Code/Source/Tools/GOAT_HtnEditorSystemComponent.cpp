
#include <AzCore/Serialization/SerializeContext.h>
#include "GOAT_HtnEditorSystemComponent.h"

#include <GOAT_Htn/GOAT_HtnTypeIds.h>

namespace GOAT_Htn
{
    AZ_COMPONENT_IMPL(GOAT_HtnEditorSystemComponent, "GOAT_HtnEditorSystemComponent",
        GOAT_HtnEditorSystemComponentTypeId, BaseSystemComponent);

    void GOAT_HtnEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_HtnEditorSystemComponent, GOAT_HtnSystemComponent>()
                ->Version(0);
        }
    }

    GOAT_HtnEditorSystemComponent::GOAT_HtnEditorSystemComponent() = default;

    GOAT_HtnEditorSystemComponent::~GOAT_HtnEditorSystemComponent() = default;

    void GOAT_HtnEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("GOAT_HtnEditorService"));
    }

    void GOAT_HtnEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("GOAT_HtnEditorService"));
    }

    void GOAT_HtnEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void GOAT_HtnEditorSystemComponent::Activate()
    {
        GOAT_HtnSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void GOAT_HtnEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        GOAT_HtnSystemComponent::Deactivate();
    }

} // namespace GOAT_Htn
