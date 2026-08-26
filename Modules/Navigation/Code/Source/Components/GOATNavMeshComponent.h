#pragma once

#include <AzCore/Component/Component.h>
#include <AzCore/Component/TickBus.h>

namespace GOAT_Navigation
{
    //! Points GOAT's path queries at the navigation mesh on this entity.
    //!
    //! RecastNavigation has no way to look a navigation mesh up, so this component is placed
    //! beside one and reports the entity it is on. That entity id is also what the navigation
    //! mesh notification bus is addressed by, so binding and listening come from the same place.
    class GOATNavMeshComponent final
        : public AZ::Component
        , private AZ::TickBus::Handler
    {
    public:
        AZ_COMPONENT_DECL(GOATNavMeshComponent);

        static void Reflect(AZ::ReflectContext* context);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    protected:
        void Activate() override;
        void Deactivate() override;

        //! AZ::TickBus. Runs once, then disconnects.
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

    private:
        //! Builds the navigation mesh once the level is running.
        //! RecastNavigation creates an empty mesh on activation and never fills it, so without
        //! this every path query fails. A project that decides for itself when to build -- a
        //! streamed world, say -- turns this off and calls UpdateNavigationMeshAsync itself.
        bool m_buildOnActivate = true;
    };
} // namespace GOAT_Navigation
