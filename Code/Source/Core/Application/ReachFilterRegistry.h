#pragma once

#include <GOAT/Interfaces/IReachFilter.h>

#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! Every way of narrowing a director's reach that a module contributed.
    //! Mirrors BackendRegistry: owned here, named by the thing that registered it, and gone
    //! again when that module unregisters, so the core never names one.
    class ReachFilterRegistry final
    {
    public:
        //! Installs a filter. Fails when its name is already taken.
        bool Register(AZStd::unique_ptr<IReachFilter> filter);

        //! Removes a filter, so a module takes its reach vocabulary with it.
        void Unregister(const AZ::Name& name);

        //! The filter of that name, or nullptr when none is registered.
        const IReachFilter* Find(const AZ::Name& name) const;

        //! Every registered filter, for console output and for the component's dropdown.
        AZStd::vector<AZ::Name> GetNames() const;


    private:
        AZStd::unordered_map<AZ::Name, AZStd::unique_ptr<IReachFilter>> m_filters;
    };
} // namespace GOAT
