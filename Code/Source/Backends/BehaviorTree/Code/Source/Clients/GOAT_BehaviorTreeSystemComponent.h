#pragma once

#include <GOAT/VocabularyScope.h>

#include <AzCore/Component/Component.h>

namespace GOAT_BehaviorTree
{
    //! Installs the behaviour tree as one of the backends an agent may name.
    class GOAT_BehaviorTreeSystemComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT_DECL(GOAT_BehaviorTreeSystemComponent);

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
        //! The words a tree is written in, taken back out when this goes.
        GOAT::VocabularyScope m_vocabulary{ "behaviour tree" };
    };
} // namespace GOAT_BehaviorTree
