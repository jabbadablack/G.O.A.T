#include <Core/Domain/BlackboardSchema.h>

#include <AzCore/std/string/conversions.h>

namespace GOAT
{
    AZ::Outcome<BlackboardKey, AZStd::string> BlackboardSchema::Declare(
        const AZ::Name& name, BlackboardScope scope, BlackboardType type, AZStd::any defaultValue)
    {
        if (name.IsEmpty())
        {
            return AZ::Failure(AZStd::string("A blackboard variable must have a name"));
        }

        if (scope >= BlackboardScope::Count || type >= BlackboardType::Count)
        {
            return AZ::Failure(AZStd::string::format("Blackboard variable '%s' has an invalid scope or type", name.GetCStr()));
        }

        if (auto existing = m_keysByName.find(name); existing != m_keysByName.end())
        {
            // Several agents commonly share one .bbx, so re-declaring a variable identically is
            // idempotent. Only a genuine disagreement about scope or type is an error.
            if (existing->second.GetScope() == scope && existing->second.GetType() == type)
            {
                return AZ::Success(existing->second);
            }

            return AZ::Failure(AZStd::string::format(
                "Blackboard variable '%s' is already declared as %s %s, and cannot be redeclared as %s %s; "
                "names are shared across every .bbx asset",
                name.GetCStr(), ToString(existing->second.GetScope()), ToString(existing->second.GetType()),
                ToString(scope), ToString(type)));
        }

        BlackboardLayout& layout = m_layouts[static_cast<size_t>(scope)];
        AZ::u32& slotCount = layout.m_slotCounts[static_cast<size_t>(type)];
        if (slotCount > BlackboardKey::MaxIndex)
        {
            return AZ::Failure(AZStd::string::format(
                "Too many %s blackboard variables of type %s", ToString(scope), ToString(type)));
        }

        const BlackboardKey key(scope, type, slotCount);
        ++slotCount;

        AZ_Assert(key.GetScope() == scope && key.GetType() == type && key.GetIndex() == slotCount - 1,
            "A packed key must round trip the scope, type and slot it was built from");

        if (!defaultValue.empty())
        {
            layout.m_defaults.emplace_back(key, AZStd::move(defaultValue));
        }

        const size_t before = m_keysByName.size();
        m_keysByName.emplace(name, key);

        AZ_Assert(m_keysByName.size() == before + 1, "Declaring a new variable must add exactly one name");
        AZ_Assert(Find(name) == key, "A declared name must resolve back to the key it was given");
        return AZ::Success(key);
    }

    BlackboardKey BlackboardSchema::Find(const AZ::Name& name) const
    {
        const auto found = m_keysByName.find(name);
        return found != m_keysByName.end() ? found->second : BlackboardKey{};
    }

    const BlackboardLayout& BlackboardSchema::GetLayout(BlackboardScope scope) const
    {
        AZ_Assert(scope < BlackboardScope::Count, "Invalid blackboard scope");
        return m_layouts[static_cast<size_t>(scope)];
    }

    void BlackboardSchema::Clear()
    {
        m_keysByName.clear();
        for (BlackboardLayout& layout : m_layouts)
        {
            layout.m_slotCounts.fill(0);
            layout.m_defaults.clear();
        }
    }
} // namespace GOAT
