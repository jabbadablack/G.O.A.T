#pragma once

#include <AzCore/Console/ILogger.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! Things a module installed, each owned here and found by the name it answers to.
    //!
    //! Backends and reach filters had this written out letter for letter, twice. The noun is
    //! passed in rather than inferred, so a message still says what was registered: "backend
    //! 'bt' is already registered" is worth more to whoever reads it than "item 'bt' is".
    template<typename T>
    class NamedRegistry final
    {
    public:
        //! @param noun what one of these is called, for the messages this reports.
        explicit NamedRegistry(const char* noun)
            : m_noun(noun)
        {
        }

        //! Installs one under its own name. Fails when that name is taken.
        bool Register(AZStd::unique_ptr<T> item)
        {
            AZ_Assert(item != nullptr, "Only something that exists can be registered");
            if (item == nullptr)
            {
                return false;
            }

            const AZ::Name name = item->GetName();
            AZ_Assert(!name.IsEmpty(), "Registering happens under a name, because that is how it is reached again");

            if (name.IsEmpty() || m_items.contains(name))
            {
                AZ_Warning("GOAT", false, "%s '%s' is already registered", m_noun, name.GetCStr());
                return false;
            }

            m_items.emplace(name, AZStd::move(item));

            AZLOG_INFO("GOAT: %s '%s' registered", m_noun, name.GetCStr());
            return true;
        }

        //! Removes one. This is what makes what a module installed leave with it.
        void Unregister(const AZ::Name& name) { m_items.erase(name); }

        //! The one registered under a name, or nullptr when there is none.
        T* Find(const AZ::Name& name) const
        {
            const auto found = m_items.find(name);
            return found != m_items.end() ? found->second.get() : nullptr;
        }

        //! Every installed name, for console output and authoring validation.
        AZStd::vector<AZ::Name> GetNames() const
        {
            AZStd::vector<AZ::Name> names;
            names.reserve(m_items.size());
            for (const auto& [name, item] : m_items)
            {
                names.push_back(name);
            }
            return names;
        }

        //! Visits each installed one. Lets a caller do something to all of them without this
        //! knowing what that something is.
        template<typename Visitor>
        void ForEach(Visitor&& visit) const
        {
            for (const auto& [name, item] : m_items)
            {
                visit(*item);
            }
        }

    private:
        AZStd::unordered_map<AZ::Name, AZStd::unique_ptr<T>> m_items;
        const char* m_noun = "item";
    };
} // namespace GOAT
