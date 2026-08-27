#include <Backends/BehaviorTree/ServiceTracker.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    void ServiceTracker::CollectDue(
        const DecisionProgram& program, DecisionCursor& cursor, DueServices& outServices) const
    {
        outServices.clear();
        AZ_Assert(!program.m_nodes.empty(), "Services are only collected from a compiled program");

        const NodeIndex leaf = cursor.GetActiveLeaf();
        if (leaf == InvalidNodeIndex)
        {
            return;
        }

        const float now = cursor.GetNow();
        for (const NodeIndex nodeIndex : program.m_serviceNodes)
        {
            AZ_Assert(nodeIndex < program.m_nodes.size(), "A service node index must address a node in the program");

            const DecisionNode& node = program.m_nodes[nodeIndex];

            // In scope means the running leaf is somewhere inside this composite's subtree.
            if (leaf < nodeIndex || leaf >= node.m_subtreeEnd)
            {
                continue;
            }

            for (AZ::u16 offset = 0; offset < node.m_serviceCount; ++offset)
            {
                const AZ::u32 service = node.m_firstService + offset;
                AZ_Assert(service < program.m_services.size(), "A service index must address a compiled service");

                float& due = cursor.Slot(static_cast<AZ::u16>(program.m_serviceSlotBase + service));
                if (due > now)
                {
                    continue;
                }

                due = now + AZStd::max(program.m_services[service].m_interval, 0.0f);
                AZ_Assert(outServices.size() < outServices.capacity(),
                "A tree cannot have more services due than it has slots for");
            outServices.push_back(service);

                AZ_Assert(due >= now, "A service's next due time must never be in the past");
            }
        }
    }
} // namespace GOAT
