#pragma once

#include <GOAT/Domain/DecisionProgram.h>

#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Where one agent is inside a compiled tree, and the per node state its walk depends on.
    //! State is indexed by node rather than kept on a stack, so bubbling a result only follows
    //! parent links and never needs a depth bounded path.
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

        //! Which child a composite is currently running.
        AZ::u16& ChildIndex(NodeIndex node) { return m_childIndex[node]; }

        //! Absolute time a cooldown expires or a time limit runs out.
        float& Deadline(NodeIndex node) { return m_deadlines[node]; }

        //! How many times a loop has repeated.
        AZ::u16& Counter(NodeIndex node) { return m_counters[node]; }

        //! Absolute time a service is next due to run.
        float& ServiceDue(AZ::u32 service) { return m_serviceDue[service]; }

        //! The agent's own clock, used for cooldowns without sweeping every node each tick.
        float GetNow() const { return m_now; }
        void AdvanceClock(float deltaTime) { m_now += deltaTime; }

    private:
        AZStd::vector<AZ::u16> m_childIndex;
        AZStd::vector<float> m_deadlines;
        AZStd::vector<AZ::u16> m_counters;
        AZStd::vector<float> m_serviceDue;
        NodeIndex m_activeLeaf = InvalidNodeIndex;
        float m_now = 0.0f;
    };
} // namespace GOAT
