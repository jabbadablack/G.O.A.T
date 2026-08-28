#pragma once

#include <GOAT_BehaviorTree/DecisionProgram.h>

#include <AzCore/std/containers/array.h>

namespace GOAT
{
    //! Where one agent is inside a compiled tree, and the per node state its walk depends on.
    //!
    //! State is addressed by the slot the compiler gave a node rather than by the node itself,
    //! because only a few nodes in a tree keep anything between ticks. Sizing it to the nodes
    //! that need one makes the whole cursor a fixed block an agent carries inline, where sizing
    //! it to the tree meant four heap arrays of mostly zeros each.
    class DecisionCursor final
    {
    public:
        //! Points the cursor at a program and rewinds it to the root.
        void Reset(const DecisionProgram& program);

        //! True when a leaf has an intent in flight.
        bool IsRunning() const { return m_activeLeaf != InvalidNodeIndex; }

        //! The leaf whose intent is in flight, or InvalidNodeIndex when idle.
        NodeIndex GetActiveLeaf() const { return m_activeLeaf; }
        void SetActiveLeaf(NodeIndex node) { m_activeLeaf = node; }

        //! The state a node keeps: a cooldown's expiry, a time limit, a loop's count, or the
        //! child a Lua composite chose. Which of those it means is the node's business.
        float& Slot(AZ::u16 slot)
        {
            AZ_Assert(slot < MaxCursorSlots, "A cursor slot must be one the compiler assigned");
            return m_slots[slot];
        }

        float GetSlot(AZ::u16 slot) const
        {
            AZ_Assert(slot < MaxCursorSlots, "A cursor slot must be one the compiler assigned");
            return m_slots[slot];
        }

        //! The agent's own clock, used for cooldowns without sweeping every node each tick.
        float GetNow() const { return m_now; }
        void AdvanceClock(float deltaTime) { m_now += deltaTime; }

    private:
        //! One value per slot. A loop count and a chosen child are whole numbers small enough to
        //! be exact in a float, so one array serves every kind of state a node keeps.
        AZStd::array<float, MaxCursorSlots> m_slots{};
        NodeIndex m_activeLeaf = InvalidNodeIndex;
        float m_now = 0.0f;
    };
} // namespace GOAT
