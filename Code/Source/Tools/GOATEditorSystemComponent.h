
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Clients/GOATSystemComponent.h>

namespace GOAT
{
    /// System component for GOAT editor
    class GOATEditorSystemComponent
        : public GOATSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = GOATSystemComponent;
    public:
        AZ_COMPONENT_DECL(GOATEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        GOATEditorSystemComponent();
        ~GOATEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
    };
} // namespace GOAT
