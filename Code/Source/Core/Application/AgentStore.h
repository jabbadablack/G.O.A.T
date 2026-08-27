#pragma once

#include <Core/Application/AgentRecord.h>

#include <GOAT/Domain/AgentId.h>

#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Every agent, addressed by the slot its handle carries.
    //!
    //! A slot's index never changes while the agent lives. That is the whole point: it lets any
    //! other per agent table be a plain array indexed by that one number rather than another hash
    //! map keyed by the same handle. A released slot becomes a hole rather than being compacted
    //! away, because compacting is exactly what would invalidate all of them at once.
    //!
    //! Records live in the array by value. Nothing in one captures its own address any more --
    //! the guard watch counts changes rather than subscribing to them -- so growing the array is
    //! free to move them, and ten thousand agents are one allocation rather than ten thousand.
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
        size_t GetSlotCount() const { return m_slots.size(); }

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

        AZStd::vector<Slot> m_slots;
        AZStd::vector<AZ::u32> m_freeSlots;
        size_t m_liveCount = 0;
    };
} // namespace GOAT
