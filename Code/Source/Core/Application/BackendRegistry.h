#pragma once

#include <GOAT/Interfaces/IBackend.h>

#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! The decision backends currently installed.
    //! Adding one is a registration; removing one is deleting its folder.
    class BackendRegistry final
    {
    public:
        //! Installs a backend under its own name. Fails when that name is taken.
        bool Register(AZStd::unique_ptr<IBackend> backend);

        //! Removes a backend. Agents planning through it fall back to the direct backend.
        void Unregister(const AZ::Name& name);

        //! The backend registered under a name, or nullptr when there is none.
        IBackend* Find(const AZ::Name& name) const;

        //! Every installed backend name, for console output and authoring validation.
        AZStd::vector<AZ::Name> GetNames() const;

        //! Removes every backend.
        void Clear();

    private:
        AZStd::unordered_map<AZ::Name, AZStd::unique_ptr<IBackend>> m_backends;
    };
} // namespace GOAT
