#include <Core/Application/BackendRegistry.h>

namespace GOAT
{
    bool BackendRegistry::Register(AZStd::unique_ptr<IBackend> backend)
    {
        if (backend == nullptr)
        {
            return false;
        }

        const AZ::Name name = backend->GetName();
        if (name.IsEmpty() || m_backends.contains(name))
        {
            AZ_Warning("GOAT", false, "Backend '%s' is already registered", name.GetCStr());
            return false;
        }

        m_backends.emplace(name, AZStd::move(backend));
        return true;
    }

    void BackendRegistry::Unregister(const AZ::Name& name)
    {
        m_backends.erase(name);
    }

    IBackend* BackendRegistry::Find(const AZ::Name& name) const
    {
        const auto found = m_backends.find(name);
        return found != m_backends.end() ? found->second.get() : nullptr;
    }

    AZStd::vector<AZ::Name> BackendRegistry::GetNames() const
    {
        AZStd::vector<AZ::Name> names;
        names.reserve(m_backends.size());
        for (const auto& [name, backend] : m_backends)
        {
            names.push_back(name);
        }
        return names;
    }

    void BackendRegistry::Clear()
    {
        m_backends.clear();
    }
} // namespace GOAT
