#pragma once

#include <Navigation/NavigationKeys.h>
#include <Navigation/NavigationService.h>
#include <Navigation/PathPool.h>

#include <GOAT/Domain/NodeType.h>
#include <GOAT/Interfaces/IActionState.h>
#include <GOAT/VocabularyScope.h>

#include <GOAT_Navigation/GOAT_NavigationBus.h>

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT_Navigation
{
    //! Owns the navigation query service and contributes this module's vocabulary to GOAT.
    //!
    //! Everything spatial lives here rather than in the core gem, so a project that never
    //! needs a navigation mesh simply does not enable this gem and never links Recast.
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
        //! GOAT_NavigationRequestBus
        void SetNavigationMesh(AZ::EntityId navMeshEntity) override;
        void ClearNavigationMesh() override;
        bool IsNavigationReady() const override;

        //! AZ::Component
        void Init() override;
        void Activate() override;
        void Deactivate() override;

        //! AZ::TickBus
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

    private:
        //! Declares this module's blackboard variables and installs its verbs and words.
        //! Fails loudly rather than half-registering, because a half-installed vocabulary
        //! produces tree compilation errors that point nowhere near the cause.
        bool InstallVocabulary();

        AZStd::unique_ptr<NavigationService> m_service;
        AZStd::unique_ptr<PathPool> m_paths;
        NavigationKeys m_keys;

        //! What this module added to the core, removed again when it is destroyed.
        GOAT::VocabularyScope m_vocabulary{"navigation"};
    };
} // namespace GOAT_Navigation
