#include <Core/Application/ActionStateRegistry.h>

#include <AzCore/Console/ILogger.h>

namespace GOAT
{
    void ActionStateRegistry::EnsureSlot(ActionStateId id)
    {
        if (id >= m_states.size())
        {
            m_states.resize(static_cast<size_t>(id) + 1);
        }

        AZ_Assert(id < m_states.size(), "Ensuring a slot must make that id addressable");
    }

    bool ActionStateRegistry::RegisterAt(ActionStateId id, AZStd::unique_ptr<IActionState> state)
    {
        AZ_Assert(state != nullptr, "A verb must exist to be registered");
        AZ_Assert(id != CoreActions::Invalid, "The invalid id is reserved and cannot hold a verb");

        if (state == nullptr || id == CoreActions::Invalid)
        {
            return false;
        }

        EnsureSlot(id);
        if (m_states[id] != nullptr)
        {
            AZ_Warning("GOAT", false, "Action verb id %u is already registered", static_cast<AZ::u32>(id));
            return false;
        }

        m_states[id] = AZStd::move(state);

        AZ_Assert(Find(id) != nullptr, "A registered verb must be findable by the id it took");
        return true;
    }

    ActionStateId ActionStateRegistry::Register(AZStd::unique_ptr<IActionState> state)
    {
        AZ_Assert(state != nullptr, "A verb must exist to be registered");
        if (state == nullptr)
        {
            return CoreActions::Invalid;
        }

        const AZ::Name name = state->GetName();
        AZ_Assert(!name.IsEmpty(), "A verb must be registered under a name, because trees reference it by one");
        if (FindId(name) != CoreActions::Invalid)
        {
            AZ_Warning("GOAT", false, "Action verb '%s' is already registered", name.GetCStr());
            return CoreActions::Invalid;
        }

        EnsureSlot(CoreActions::FirstRegistered);
        for (size_t id = CoreActions::FirstRegistered; id < m_states.size(); ++id)
        {
            if (m_states[id] == nullptr)
            {
                m_states[id] = AZStd::move(state);
                AZLOG_INFO("GOAT: verb '%s' registered as id %zu", name.GetCStr(), id);
                return static_cast<ActionStateId>(id);
            }
        }

        if (m_states.size() > AZStd::numeric_limits<ActionStateId>::max())
        {
            AZ_Warning("GOAT", false, "No action verb ids left for '%s'", name.GetCStr());
            return CoreActions::Invalid;
        }

        m_states.push_back(AZStd::move(state));

        AZLOG_INFO("GOAT: verb '%s' registered as id %zu", name.GetCStr(), m_states.size() - 1);
        return static_cast<ActionStateId>(m_states.size() - 1);
    }

    void ActionStateRegistry::Unregister(ActionStateId id)
    {
        AZ_Assert(id < m_states.size(), "Unregistering a verb id that was never registered");
        if (id < m_states.size())
        {
            m_states[id].reset();
        }

        AZ_Assert(Find(id) == nullptr, "An unregistered verb must no longer be findable");
    }

    ActionStateId ActionStateRegistry::FindId(const AZ::Name& name) const
    {
        for (size_t id = 0; id < m_states.size(); ++id)
        {
            if (m_states[id] != nullptr && m_states[id]->GetName() == name)
            {
                return static_cast<ActionStateId>(id);
            }
        }
        return CoreActions::Invalid;
    }

    IActionState* ActionStateRegistry::Find(ActionStateId id) const
    {
        return id < m_states.size() ? m_states[id].get() : nullptr;
    }

    AZStd::vector<AZ::Name> ActionStateRegistry::GetNames() const
    {
        AZStd::vector<AZ::Name> names;
        for (const auto& state : m_states)
        {
            if (state != nullptr)
            {
                names.push_back(state->GetName());
            }
        }
        return names;
    }
} // namespace GOAT
