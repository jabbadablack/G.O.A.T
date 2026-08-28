
#include <AzCore/Serialization/SerializeContext.h>
#include "GOAT_BehaviorTreeEditorSystemComponent.h"

#include <GOAT_BehaviorTree/GOAT_BehaviorTreeTypeIds.h>

namespace GOAT_BehaviorTree
{
    AZ_COMPONENT_IMPL(GOAT_BehaviorTreeEditorSystemComponent, "GOAT_BehaviorTreeEditorSystemComponent",
        GOAT_BehaviorTreeEditorSystemComponentTypeId, BaseSystemComponent);

    void GOAT_BehaviorTreeEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOAT_BehaviorTreeEditorSystemComponent, GOAT_BehaviorTreeSystemComponent>()
                ->Version(0);
        }
    }

    GOAT_BehaviorTreeEditorSystemComponent::GOAT_BehaviorTreeEditorSystemComponent() = default;

    GOAT_BehaviorTreeEditorSystemComponent::~GOAT_BehaviorTreeEditorSystemComponent() = default;

    void GOAT_BehaviorTreeEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("GOAT_BehaviorTreeEditorService"));
    }

    void GOAT_BehaviorTreeEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("GOAT_BehaviorTreeEditorService"));
    }

    void GOAT_BehaviorTreeEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void GOAT_BehaviorTreeEditorSystemComponent::Activate()
    {
        GOAT_BehaviorTreeSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void GOAT_BehaviorTreeEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        GOAT_BehaviorTreeSystemComponent::Deactivate();
    }

} // namespace GOAT_BehaviorTree
