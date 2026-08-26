#pragma once

#include <GOAT/Domain/NodeType.h>
#include <GOAT/Interfaces/IActionState.h>

#include <GOAT_Animation/GOAT_AnimationBus.h>

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
        , protected GOAT_AnimationRequestBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(GOAT_AnimationSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        GOAT_AnimationSystemComponent();
        ~GOAT_AnimationSystemComponent();

    protected:
        //! AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;

    private:
        //! Installs this module's verbs and the words that run them.
        bool InstallVocabulary();

        //! Takes them back out again, so disabling the gem really does remove the vocabulary.
        void RemoveVocabulary();

        //! Registers one verb together with the word that runs it.
        bool InstallVerb(
            AZStd::unique_ptr<GOAT::IActionState> action, GOAT::NodeTypeDescriptor descriptor);

        AZStd::vector<GOAT::ActionStateId> m_installedActions;
        AZStd::vector<AZ::Name> m_installedNodeTypes;
    };
} // namespace GOAT_Animation
