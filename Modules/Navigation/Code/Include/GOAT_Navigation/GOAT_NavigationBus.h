
#pragma once

#include <GOAT_Navigation/GOAT_NavigationTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace GOAT_Navigation
{
    class GOAT_NavigationRequests
    {
    public:
        AZ_RTTI(GOAT_NavigationRequests, GOAT_NavigationRequestsTypeId);
        virtual ~GOAT_NavigationRequests() = default;
        // Put your public methods here
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
