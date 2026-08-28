#pragma once

#include <AzCore/Component/Component.h>

namespace GOAT_Htn
{
    //! Installs the task network as one of the backends an agent may name.
    class GOAT_HtnSystemComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT_DECL(GOAT_HtnSystemComponent);

        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

    protected:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component
        void Activate() override;
        void Deactivate() override;
        ////////////////////////////////////////////////////////////////////////
    };
} // namespace GOAT_Htn
