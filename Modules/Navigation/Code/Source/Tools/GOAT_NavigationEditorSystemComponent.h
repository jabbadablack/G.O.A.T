
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Clients/GOAT_NavigationSystemComponent.h>

namespace GOAT_Navigation
{
    /// System component for GOAT_Navigation editor
    class GOAT_NavigationEditorSystemComponent
        : public GOAT_NavigationSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = GOAT_NavigationSystemComponent;
    public:
        AZ_COMPONENT_DECL(GOAT_NavigationEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        GOAT_NavigationEditorSystemComponent();
        ~GOAT_NavigationEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
    };
} // namespace GOAT_Navigation
