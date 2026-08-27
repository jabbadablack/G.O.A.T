#include <Core/Frontend/DecisionCursor.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    void DecisionCursor::Reset(const DecisionProgram& program)
    {
        AZ_Assert(!program.m_nodes.empty(), "A cursor is only ever reset onto a compiled program");

        const size_t nodeCount = program.m_nodes.size();
        m_childIndex.assign(nodeCount, 0);
        m_deadlines.assign(nodeCount, 0.0f);
        m_counters.assign(nodeCount, 0);
        m_serviceDue.assign(program.m_services.size(), 0.0f);
        m_activeLeaf = InvalidNodeIndex;
        m_now = 0.0f;

        AZ_Assert(m_childIndex.size() == nodeCount && m_deadlines.size() == nodeCount && m_counters.size() == nodeCount,
            "Every per node cursor array must cover the whole program");
    }
} // namespace GOAT
