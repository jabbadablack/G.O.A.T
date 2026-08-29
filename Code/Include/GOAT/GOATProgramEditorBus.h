#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! Drives the program editor window from outside it, which is how the asset browser
    //! opens what it created.
    class GOATProgramEditorRequests
        : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

        virtual ~GOATProgramEditorRequests() = default;

        //! Opens a .goat file, replacing whatever the window was showing.
        virtual void OpenProgram(const AZStd::string& fullPath) = 0;
    };

    using GOATProgramEditorRequestBus = AZ::EBus<GOATProgramEditorRequests>;
} // namespace GOAT
