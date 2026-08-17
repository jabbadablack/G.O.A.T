
#pragma once

#include <GOAT/GOATTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace GOAT
{
    class GOATRequests
    {
    public:
        AZ_RTTI(GOATRequests, GOATRequestsTypeId);
        virtual ~GOATRequests() = default;
        // Put your public methods here
    };

    class GOATBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using GOATRequestBus = AZ::EBus<GOATRequests, GOATBusTraits>;
    using GOATInterface = AZ::Interface<GOATRequests>;

} // namespace GOAT
