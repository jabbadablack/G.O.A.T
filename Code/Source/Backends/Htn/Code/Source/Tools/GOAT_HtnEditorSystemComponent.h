
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <Clients/GOAT_HtnSystemComponent.h>

namespace GOAT_Htn
{
    /// System component for GOAT_Htn editor
    class GOAT_HtnEditorSystemComponent
        : public GOAT_HtnSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
    {
        using BaseSystemComponent = GOAT_HtnSystemComponent;
    public:
        AZ_COMPONENT_DECL(GOAT_HtnEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        GOAT_HtnEditorSystemComponent();
        ~GOAT_HtnEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        // AZ::Component
        void Activate() override;
        void Deactivate() override;
    };
} // namespace GOAT_Htn
