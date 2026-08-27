#include <Core/Application/AgentStore.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/std/algorithm.h>

namespace GOAT
{
    AgentId AgentStore::Acquire(AgentRecord&& record)
    {
        AZ::u32 slot = 0;
        if (!m_freeSlots.empty())
        {
            slot = m_freeSlots.back();
            m_freeSlots.pop_back();
        }
        else
        {
            // Capacity doubles rather than fitting each arrival, or registering N agents would
            // copy every record already there N times.
            slot = static_cast<AZ::u32>(m_slots.size());
            if (m_slots.size() == m_slots.capacity())
            {
                m_slots.reserve(AZStd::max(size_t(16), m_slots.capacity() * 2));
            }
            m_slots.emplace_back();
        }

        m_slots[slot].m_record = AZStd::move(record);
        m_slots[slot].m_live = true;
        ++m_liveCount;

        return AgentId(slot, m_slots[slot].m_generation);
    }

    void AgentStore::Reserve(size_t count)
    {
        // Holes count: they are slots a registration can take without growing anything.
        const size_t needed = m_liveCount + count;
        if (needed > m_slots.size() + m_freeSlots.size())
        {
            m_slots.reserve(needed - m_freeSlots.size());
        }
    }

    bool AgentStore::Release(AgentId agent)
    {
        if (Find(agent) == nullptr)
        {
            return false;
        }

        Slot& entry = m_slots[agent.GetIndex()];

        // Emptied rather than destroyed, so the slot's own buffers go back to the free list with
        // it and the next agent here does not allocate them again. Written out rather than
        // braced: AZ::EntityId's default constructor is explicit.
        AgentRecord empty;
        entry.m_record = AZStd::move(empty);
        entry.m_live = false;

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
        return entry.m_live && entry.m_generation == agent.GetGeneration() ? &entry.m_record : nullptr;
    }

    AgentId AgentStore::GetHandleAt(size_t slot) const
    {
        AZ_Assert(slot < m_slots.size(), "A slot index must address a slot the store has");
        if (slot >= m_slots.size() || !m_slots[slot].m_live)
        {
            return AgentId{};
        }

        return AgentId(static_cast<AZ::u32>(slot), m_slots[slot].m_generation);
    }
} // namespace GOAT
