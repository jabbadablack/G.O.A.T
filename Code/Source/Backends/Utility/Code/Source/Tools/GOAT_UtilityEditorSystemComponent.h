
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Clients/GOAT_UtilitySystemComponent.h>

namespace GOAT_Utility
{
    /// System component for GOAT_Utility editor
    class GOAT_UtilityEditorSystemComponent
        : public GOAT_UtilitySystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = GOAT_UtilitySystemComponent;
    public:
        AZ_COMPONENT_DECL(GOAT_UtilityEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        GOAT_UtilityEditorSystemComponent();
        ~GOAT_UtilityEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
    };
} // namespace GOAT_Utility
