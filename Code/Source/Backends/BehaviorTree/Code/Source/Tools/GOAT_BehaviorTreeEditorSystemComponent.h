
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Clients/GOAT_BehaviorTreeSystemComponent.h>

namespace GOAT_BehaviorTree
{
    /// System component for GOAT_BehaviorTree editor
    class GOAT_BehaviorTreeEditorSystemComponent
        : public GOAT_BehaviorTreeSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = GOAT_BehaviorTreeSystemComponent;
    public:
        AZ_COMPONENT_DECL(GOAT_BehaviorTreeEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        GOAT_BehaviorTreeEditorSystemComponent();
        ~GOAT_BehaviorTreeEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
    };
} // namespace GOAT_BehaviorTree
