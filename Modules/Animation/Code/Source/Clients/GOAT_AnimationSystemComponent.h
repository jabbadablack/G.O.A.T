#pragma once

#include <GOAT/Domain/NodeType.h>
#include <GOAT/Interfaces/IActionState.h>


#include <GOAT/VocabularyScope.h>

#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT_Animation
{
    //! Contributes this module's animation vocabulary to GOAT.
    //!
    //! Only this gem knows EMotionFX exists. A project that animates some other way, or does not
    //! animate at all, simply does not enable it and the words disappear with it.
    class GOAT_AnimationSystemComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT_DECL(GOAT_AnimationSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);


    protected:
        //! AZ::Component
        void Activate() override;
        void Deactivate() override;

    private:
        //! Installs this module's verbs and the words that run them.
        bool InstallVocabulary();



        //! What this module added to the core, removed again when it is destroyed.
        GOAT::VocabularyScope m_vocabulary{"animation"};
    };
} // namespace GOAT_Animation
