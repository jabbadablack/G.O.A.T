
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Clients/GOAT_SmartObjectSystemComponent.h>

namespace GOAT_SmartObject
{
    /// System component for GOAT_SmartObject editor
    class GOAT_SmartObjectEditorSystemComponent
        : public GOAT_SmartObjectSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = GOAT_SmartObjectSystemComponent;
    public:
        AZ_COMPONENT_DECL(GOAT_SmartObjectEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        GOAT_SmartObjectEditorSystemComponent();
        ~GOAT_SmartObjectEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
    };
} // namespace GOAT_SmartObject
