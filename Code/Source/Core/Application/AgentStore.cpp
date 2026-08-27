#include <Core/Application/AgentStore.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    AgentId AgentStore::Acquire(AZStd::unique_ptr<AgentRecord> record)
    {
        AZ_Assert(record != nullptr, "A slot is only taken for a record that exists");

        AZ::u32 slot = 0;
        if (!m_freeSlots.empty())
        {
            slot = m_freeSlots.back();
            m_freeSlots.pop_back();
        }
        else
        {
            slot = static_cast<AZ::u32>(m_slots.size());
            m_slots.push_back(Slot{});
        }

        m_slots[slot].m_record = AZStd::move(record);
        ++m_liveCount;

        return AgentId(slot, m_slots[slot].m_generation);
    }

    bool AgentStore::Release(AgentId agent)
    {
        if (Find(agent) == nullptr)
        {
            return false;
        }

        Slot& entry = m_slots[agent.GetIndex()];
        entry.m_record.reset();

        // Bumped before the slot is offered again, so every handle to the agent that just left
        // stops resolving the moment it leaves rather than when its replacement arrives.
        ++entry.m_generation;
        m_freeSlots.push_back(agent.GetIndex());
        --m_liveCount;

        return true;
    }

    AgentRecord* AgentStore::Find(AgentId agent)
    {
        return const_cast<AgentRecord*>(const_cast<const AgentStore*>(this)->Find(agent));
    }

    const AgentRecord* AgentStore::Find(AgentId agent) const
    {
        if (agent.IsNull() || agent.GetIndex() >= m_slots.size())
        {
            return nullptr;
        }

        const Slot& entry = m_slots[agent.GetIndex()];
        return entry.m_generation == agent.GetGeneration() ? entry.m_record.get() : nullptr;
    }

    AgentId AgentStore::GetHandleAt(size_t slot) const
    {
        AZ_Assert(slot < m_slots.size(), "A slot index must address a slot the store has");
        if (slot >= m_slots.size() || m_slots[slot].m_record == nullptr)
        {
            return AgentId{};
        }

        return AgentId(static_cast<AZ::u32>(slot), m_slots[slot].m_generation);
    }
} // namespace GOAT
