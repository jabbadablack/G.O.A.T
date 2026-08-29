#pragma once

#include <GOAT/VocabularyScope.h>

#include <AzCore/Component/Component.h>

namespace GOAT_Utility
{
    //! Installs the task network as one of the backends an agent may name.
    class GOAT_UtilitySystemComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT_DECL(GOAT_UtilitySystemComponent);

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

    private:
        GOAT::VocabularyScope m_vocabulary{ "utility" };
    };
} // namespace GOAT_Utility
