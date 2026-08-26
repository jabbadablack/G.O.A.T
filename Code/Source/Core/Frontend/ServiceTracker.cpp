#include <Core/Frontend/ServiceTracker.h>

namespace GOAT
{
    void ServiceTracker::CollectDue(
        const DecisionProgram& program, DecisionCursor& cursor, AZStd::vector<AZ::u32>& outServices) const
    {
        outServices.clear();

        const NodeIndex leaf = cursor.GetActiveLeaf();
        if (leaf == InvalidNodeIndex)
        {
            return;
        }

        const float now = cursor.GetNow();
        for (const NodeIndex nodeIndex : program.m_serviceNodes)
        {
            const DecisionNode& node = program.m_nodes[nodeIndex];

            // In scope means the running leaf is somewhere inside this composite's subtree.
            if (leaf < nodeIndex || leaf >= node.m_subtreeEnd)
            {
                continue;
            }

            for (AZ::u16 offset = 0; offset < node.m_serviceCount; ++offset)
            {
                const AZ::u32 service = node.m_firstService + offset;
                float& due = cursor.ServiceDue(service);
                if (due > now)
                {
                    continue;
                }

                due = now + AZStd::max(program.m_services[service].m_interval, 0.0f);
                outServices.push_back(service);
            }
        }
    }
} // namespace GOAT
