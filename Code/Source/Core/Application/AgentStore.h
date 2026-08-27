#pragma once

#include <Core/Application/AgentRecord.h>

#include <GOAT/Domain/AgentId.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! Every agent, addressed by the slot its handle carries.
    //!
    //! A slot's index never changes while the agent lives. That is the whole point: it lets any
    //! other per agent table be a plain array indexed by that one number rather than another hash
    //! map keyed by the same handle. A released slot becomes a hole rather than being compacted
    //! away, because compacting is exactly what would invalidate all of them at once.
    //!
    //! Records live by value in chunks that are never resized, so a record's address never moves
    //! once handed out. A tick holds one record for the whole of it, and a behaviour that
    //! registers an agent part way through must not pull that record out from under the tick
    //! running it. PlanStore hands out spans from fixed chunks for exactly the same reason.
    //!
    //! One allocation per chunk rather than one per agent, and the records in a chunk sit
    //! together, so walking a band still reads them in order.
    //!
    //! Holes are bounded by the peak number of live agents, which for a crowd is the right
    //! bound. Iterating asks for a slot at a time and skips the empty ones.
    class AgentStore final
    {
    public:
        //! Takes ownership of a record and returns the handle that addresses it.
        AgentId Acquire(AgentRecord&& record);

        //! Makes room for count more agents, so a burst of registrations grows the array once
        //! rather than once per agent.
        void Reserve(size_t count);

        //! Destroys the record a handle addresses and frees its slot for reuse.
        //! False when the handle addresses nothing, which is what a stale one does.
        bool Release(AgentId agent);

        //! The record a handle addresses, or nullptr when that handle is stale or null.
        AgentRecord* Find(AgentId agent);
        const AgentRecord* Find(AgentId agent) const;

        //! Slots ever allocated, holes included. This is what an iteration runs to.
        size_t GetSlotCount() const { return m_slotCount; }

        //! The handle in a slot, or a null handle when that slot is a hole.
        AgentId GetHandleAt(size_t slot) const;

        //! How many agents are actually registered.
        size_t Size() const { return m_liveCount; }

    private:
        //! One agent's place.
        struct Slot final
        {
            AgentRecord m_record;
            //! False when the slot is a hole.
            bool m_live = false;
            //! Bumped when the slot is reused, so a handle to the previous occupant resolves to
            //! nothing rather than to whoever took its place.
            AZ::u32 m_generation = 1;
        };

        //! Records per chunk. Big enough that a level of any size needs few chunks, small enough
        //! that one chunk is a handful of pages.
        static constexpr size_t RecordsPerChunk = 256;

        struct Chunk final
        {
            AZ_CLASS_ALLOCATOR(Chunk, AZ::SystemAllocator);
            AZStd::array<Slot, RecordsPerChunk> m_slots;
        };

        //! The slot at an index, or nullptr when no chunk holds it yet.
        Slot* At(size_t slot);
        const Slot* At(size_t slot) const;

        AZStd::vector<AZStd::unique_ptr<Chunk>> m_chunks;
        AZStd::vector<AZ::u32> m_freeSlots;
        size_t m_slotCount = 0;
        size_t m_liveCount = 0;
    };
} // namespace GOAT
