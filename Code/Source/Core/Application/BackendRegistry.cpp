#include <Core/Application/BackendRegistry.h>

#include <AzCore/Console/ILogger.h>

namespace GOAT
{
    bool BackendRegistry::Register(AZStd::unique_ptr<IBackend> backend)
    {
        AZ_Assert(backend != nullptr, "A backend must exist to be registered");
        if (backend == nullptr)
        {
            return false;
        }

        const AZ::Name name = backend->GetName();
        AZ_Assert(!name.IsEmpty(), "A backend must be registered under a name, because trees delegate to it by one");
        if (name.IsEmpty() || m_backends.contains(name))
        {
            AZ_Warning("GOAT", false, "Backend '%s' is already registered", name.GetCStr());
            return false;
        }

        m_backends.emplace(name, AZStd::move(backend));

        AZLOG_INFO("GOAT: backend '%s' registered", name.GetCStr());
        AZ_Assert(Find(name) != nullptr, "A registered backend must be findable by its name");
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

    void BackendRegistry::ReleaseAgent(const PlanContext& context) const
    {
        for (auto& [name, backend] : m_backends)
        {
            backend->Release(context);
        }
    }
} // namespace GOAT
