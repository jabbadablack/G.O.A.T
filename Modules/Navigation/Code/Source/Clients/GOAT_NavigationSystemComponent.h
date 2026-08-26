
#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <GOAT_Navigation/GOAT_NavigationBus.h>

namespace GOAT_Navigation
{
    class GOAT_NavigationSystemComponent
        : public AZ::Component
        , protected GOAT_NavigationRequestBus::Handler
        , public AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(GOAT_NavigationSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        GOAT_NavigationSystemComponent();
        ~GOAT_NavigationSystemComponent();

    protected:
        ////////////////////////////////////////////////////////////////////////
        // GOAT_NavigationRequestBus interface implementation

        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override;
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////

        ////////////////////////////////////////////////////////////////////////
        // AZTickBus interface implementation
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;
        ////////////////////////////////////////////////////////////////////////
    };

} // namespace GOAT_Navigation
