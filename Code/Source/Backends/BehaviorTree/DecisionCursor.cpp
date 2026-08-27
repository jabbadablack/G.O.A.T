#include <Backends/BehaviorTree/DecisionCursor.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    void DecisionCursor::Reset(const DecisionProgram& program)
    {
        AZ_Assert(!program.m_nodes.empty(), "A cursor is only ever reset onto a compiled program");
        AZ_Assert(program.m_cursorSlotCount <= MaxCursorSlots,
            "A compiled program never needs more cursor slots than an agent can carry");

        m_slots.fill(0.0f);
        m_activeLeaf = InvalidNodeIndex;
        m_now = 0.0f;
    }
} // namespace GOAT
