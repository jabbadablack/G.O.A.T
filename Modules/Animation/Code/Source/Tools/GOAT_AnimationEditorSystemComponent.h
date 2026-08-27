
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Clients/GOAT_AnimationSystemComponent.h>

namespace GOAT_Animation
{
    /// System component for GOAT_Animation editor
    class GOAT_AnimationEditorSystemComponent
        : public GOAT_AnimationSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = GOAT_AnimationSystemComponent;
    public:
        AZ_COMPONENT_DECL(GOAT_AnimationEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        GOAT_AnimationEditorSystemComponent();
        ~GOAT_AnimationEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
    };
} // namespace GOAT_Animation
