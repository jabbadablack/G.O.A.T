#pragma once

#include <SmartObject/SmartObjectActions.h>
#include <SmartObject/SmartObjectRegistry.h>

#include <GOAT/Domain/NodeType.h>
#include <GOAT/Interfaces/IActionState.h>

#include <GOAT_SmartObject/GOAT_SmartObjectBus.h>

#include <GOAT/VocabularyScope.h>

#include <AzCore/Component/Component.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT_SmartObject
{
    //! Owns the smart object registry and contributes this module's vocabulary to GOAT.
    class GOAT_SmartObjectSystemComponent
        : public AZ::Component
        , protected GOAT_SmartObjectRequestBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(GOAT_SmartObjectSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        GOAT_SmartObjectSystemComponent();
        ~GOAT_SmartObjectSystemComponent();

    protected:
        //! GOAT_SmartObjectRequestBus
        void RegisterObject(AZ::EntityId entity, SmartObjectDescription description) override;
        void UnregisterObject(AZ::EntityId entity) override;
        SmartObjectClaim Claim(
            GOAT::AgentId agent, const AZ::Name& use, const AZ::Vector3& from, float radius) override;
        void Release(GOAT::AgentId agent) override;
        AZ::u32 GetFreeSlots(AZ::EntityId entity) const override;

        //! AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;

    private:
        //! Declares this module's blackboard variables and installs its verbs and words.
        bool InstallVocabulary();


        //! Registers one verb together with the word that runs it.
        bool InstallVerb(AZStd::unique_ptr<GOAT::IActionState> action, GOAT::NodeTypeDescriptor descriptor);

        AZStd::unique_ptr<SmartObjectRegistry> m_registry;
        SmartObjectKeys m_keys;

        //! What this module added to the core, removed again when it is destroyed.
        GOAT::VocabularyScope m_vocabulary{"smart object"};
    };
} // namespace GOAT_SmartObject
