#include <Core/Application/ReachFilterRegistry.h>

#include <AzCore/Console/ILogger.h>

namespace GOAT
{
    bool ReachFilterRegistry::Register(AZStd::unique_ptr<IReachFilter> filter)
    {
        AZ_Assert(filter != nullptr, "A reach filter must exist to be registered");
        if (filter == nullptr)
        {
            return false;
        }

        const AZ::Name name = filter->GetName();
        AZ_Assert(!name.IsEmpty(), "A reach filter must be registered under a name, because a director names one");

        if (name.IsEmpty() || m_filters.contains(name))
        {
            AZ_Warning("GOAT", false, "Reach filter '%s' is already registered", name.GetCStr());
            return false;
        }

        m_filters.emplace(name, AZStd::move(filter));

        AZLOG_INFO("GOAT: reach filter '%s' registered", name.GetCStr());
        AZ_Assert(Find(name) != nullptr, "A registered reach filter must be findable by its name");
        return true;
    }

    void ReachFilterRegistry::Unregister(const AZ::Name& name)
    {
        m_filters.erase(name);
        AZ_Assert(Find(name) == nullptr, "An unregistered reach filter must no longer be findable");
    }

    const IReachFilter* ReachFilterRegistry::Find(const AZ::Name& name) const
    {
        const auto found = m_filters.find(name);
        return found != m_filters.end() ? found->second.get() : nullptr;
    }

    AZStd::vector<AZ::Name> ReachFilterRegistry::GetNames() const
    {
        AZStd::vector<AZ::Name> names;
        names.reserve(m_filters.size());
        for (const auto& [name, filter] : m_filters)
        {
            names.push_back(name);
        }

        AZ_Assert(names.size() == m_filters.size(), "Listing filters must report exactly as many as exist");
        return names;
    }

    void ReachFilterRegistry::Clear()
    {
        m_filters.clear();
        AZ_Assert(m_filters.empty(), "Clearing must leave no reach filter installed");
    }
} // namespace GOAT
