#include <Core/Application/AgentStore.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    AgentStore::Slot* AgentStore::At(size_t slot)
    {
        return const_cast<Slot*>(const_cast<const AgentStore*>(this)->At(slot));
    }

    const AgentStore::Slot* AgentStore::At(size_t slot) const
    {
        const size_t chunk = slot / RecordsPerChunk;
        if (chunk >= m_chunks.size())
        {
            return nullptr;
        }

        return &m_chunks[chunk]->m_slots[slot % RecordsPerChunk];
    }

    void AgentStore::Reserve(size_t count)
    {
        const size_t needed = m_slotCount + count;
        const size_t chunks = (needed + RecordsPerChunk - 1) / RecordsPerChunk;
        while (m_chunks.size() < chunks)
        {
            m_chunks.push_back(AZStd::make_unique<Chunk>());
        }
    }

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
            slot = static_cast<AZ::u32>(m_slotCount);
            if (At(slot) == nullptr)
            {
                m_chunks.push_back(AZStd::make_unique<Chunk>());
            }
            ++m_slotCount;
        }

        Slot* entry = At(slot);
        AZ_Assert(entry != nullptr, "A slot just taken must belong to a chunk");

        entry->m_record = AZStd::move(record);
        entry->m_live = true;
        ++m_liveCount;

        return AgentId(slot, entry->m_generation);
    }

    bool AgentStore::Release(AgentId agent)
    {
        if (Find(agent) == nullptr)
        {
            return false;
        }

        Slot* entry = At(agent.GetIndex());

        // Emptied rather than destroyed, so the slot's own buffers go back to the free list with
        // it and the next agent here does not allocate them again. Written out rather than
        // braced: AZ::EntityId's default constructor is explicit.
        AgentRecord empty;
        entry->m_record = AZStd::move(empty);
        entry->m_live = false;

        // Bumped before the slot is offered again, so every handle to the agent that just left
        // stops resolving the moment it leaves rather than when its replacement arrives.
        ++entry->m_generation;
        m_freeSlots.push_back(agent.GetIndex());
        --m_liveCount;

        // Compact trailing free slots to keep GetSlotCount bounded by the highest live slot.
        // This is opportunistic: only removes slots at the end so existing slot indices stay
        // stable and no handles are invalidated unexpectedly.
        while (m_slotCount > 0)
        {
            const size_t lastIndex = m_slotCount - 1;
            const Slot* lastEntry = At(lastIndex);
            if (lastEntry != nullptr && !lastEntry->m_live)
            {
                // Remove any occurrence of this index from the free list so it won't be reused.
                const AZ::u32 lastIdx32 = static_cast<AZ::u32>(lastIndex);
                auto it = AZStd::find(m_freeSlots.begin(), m_freeSlots.end(), lastIdx32);
                if (it != m_freeSlots.end())
                {
                    m_freeSlots.erase(it);
                }

                --m_slotCount;

                // If the last chunk became unused, pop it. Chunks partition slots so chunk
                // boundaries are at multiples of RecordsPerChunk.
                const size_t chunkIndex = lastIndex / RecordsPerChunk;
                const size_t chunkStart = chunkIndex * RecordsPerChunk;
                if (m_slotCount <= chunkStart && !m_chunks.empty())
                {
                    m_chunks.pop_back();
                }

                // Continue trimming while trailing slots remain free.
                continue;
            }
            break;
        }

        return true;
    }

    AgentRecord* AgentStore::Find(AgentId agent)
    {
        return const_cast<AgentRecord*>(const_cast<const AgentStore*>(this)->Find(agent));
    }

    const AgentRecord* AgentStore::Find(AgentId agent) const
    {
        if (agent.IsNull() || agent.GetIndex() >= m_slotCount)
        {
            return nullptr;
        }

        const Slot* entry = At(agent.GetIndex());
        if (entry == nullptr || !entry->m_live || entry->m_generation != agent.GetGeneration())
        {
            return nullptr;
        }

        return &entry->m_record;
    }

    AgentId AgentStore::GetHandleAt(size_t slot) const
    {
        AZ_Assert(slot < m_slotCount, "A slot index must address a slot the store has");

        const Slot* entry = slot < m_slotCount ? At(slot) : nullptr;
        if (entry == nullptr || !entry->m_live)
        {
            return AgentId{};
        }

        return AgentId(static_cast<AZ::u32>(slot), entry->m_generation);
    }
} // namespace GOAT
