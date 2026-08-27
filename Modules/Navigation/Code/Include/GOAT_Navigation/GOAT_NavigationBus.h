
#pragma once

#include <GOAT_Navigation/GOAT_NavigationTypeIds.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace GOAT_Navigation
{
    class GOAT_NavigationRequests
    {
    public:
        AZ_RTTI(GOAT_NavigationRequests, GOAT_NavigationRequestsTypeId);
        virtual ~GOAT_NavigationRequests() = default;

        //! Points path queries at a navigation mesh. Calling again rebinds to the new one.
        //! Sent by the component sitting on the navigation mesh entity, which is how the
        //! mesh is discovered: RecastNavigation offers no lookup of its own.
        virtual void SetNavigationMesh(AZ::EntityId navMeshEntity) = 0;

        //! Drops the current binding and cancels every query in flight.
        virtual void ClearNavigationMesh() = 0;

        //! True once a mesh is bound and its worker queries are usable.
        virtual bool IsNavigationReady() const = 0;
    };

    class GOAT_NavigationBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using GOAT_NavigationRequestBus = AZ::EBus<GOAT_NavigationRequests, GOAT_NavigationBusTraits>;
    using GOAT_NavigationInterface = AZ::Interface<GOAT_NavigationRequests>;

} // namespace GOAT_Navigation
