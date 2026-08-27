
#pragma once

#include <GOAT_Animation/GOAT_AnimationTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace GOAT_Animation
{
    class GOAT_AnimationRequests
    {
    public:
        AZ_RTTI(GOAT_AnimationRequests, GOAT_AnimationRequestsTypeId);
        virtual ~GOAT_AnimationRequests() = default;
        // Put your public methods here
    };

    class GOAT_AnimationBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using GOAT_AnimationRequestBus = AZ::EBus<GOAT_AnimationRequests, GOAT_AnimationBusTraits>;
    using GOAT_AnimationInterface = AZ::Interface<GOAT_AnimationRequests>;

} // namespace GOAT_Animation
