#pragma once

#include <GOAT/Interfaces/IDecisionBackend.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! Installs and removes the backends that decide how agents act.
    class GOATBackendRequests
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~GOATBackendRequests() = default;

        //! Installs a backend, taking it from the caller. Fails when the name is taken.
        virtual bool RegisterDecisionBackend(AZStd::unique_ptr<IDecisionBackend>& backend) = 0;

        //! Removes a backend by name.
        virtual void UnregisterDecisionBackend(const AZ::Name& name) = 0;

        //! The backend registered under a name, or nullptr.
        virtual IDecisionBackend* FindDecisionBackend(const AZ::Name& name) const = 0;

        //! Every installed backend name.
        virtual AZStd::vector<AZ::Name> GetDecisionBackendNames() const = 0;
    };

    using GOATBackendRequestBus = AZ::EBus<GOATBackendRequests>;
} // namespace GOAT
