
#include <AzCore/Serialization/SerializeContext.h>
#include "GOAT_AnimationEditorSystemComponent.h"

#include <GOAT_Animation/GOAT_AnimationTypeIds.h>

namespace GOAT_Animation
{
    AZ_COMPONENT_IMPL(GOAT_AnimationEditorSystemComponent, "GOAT_AnimationEditorSystemComponent",
        GOAT_AnimationEditorSystemComponentTypeId, BaseSystemComponent);

    void GOAT_AnimationEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_AnimationEditorSystemComponent, GOAT_AnimationSystemComponent>()
                ->Version(0);
        }
    }

    GOAT_AnimationEditorSystemComponent::GOAT_AnimationEditorSystemComponent() = default;

    GOAT_AnimationEditorSystemComponent::~GOAT_AnimationEditorSystemComponent() = default;

    void GOAT_AnimationEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("GOAT_AnimationEditorService"));
    }

    void GOAT_AnimationEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("GOAT_AnimationEditorService"));
    }

    void GOAT_AnimationEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void GOAT_AnimationEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

    void GOAT_AnimationEditorSystemComponent::Activate()
    {
        GOAT_AnimationSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void GOAT_AnimationEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        GOAT_AnimationSystemComponent::Deactivate();
    }

} // namespace GOAT_Animation
