#include <Core/Frontend/DecisionCursor.h>

namespace GOAT
{
    void DecisionCursor::Reset(const DecisionProgram& program)
    {
        const size_t nodeCount = program.m_nodes.size();
        m_childIndex.assign(nodeCount, 0);
        m_deadlines.assign(nodeCount, 0.0f);
        m_counters.assign(nodeCount, 0);
        m_serviceDue.assign(program.m_services.size(), 0.0f);
        m_activeLeaf = InvalidNodeIndex;
        m_now = 0.0f;
    }
} // namespace GOAT
