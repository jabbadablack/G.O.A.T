#include <Core/Frontend/GuardEvaluator.h>

#include <Core/Frontend/NodePredicate.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    AbortDecision GuardEvaluator::Evaluate(
        const DecisionProgram& program, const DecisionCursor& cursor, const PlanContext& context) const
    {
        AZ_Assert(!program.m_nodes.empty(), "Guards are only evaluated against a compiled program");
        AZ_Assert(program.m_guardNodes.size() <= program.m_nodes.size(),
            "A program cannot hold more guards than it has nodes");

        AbortDecision decision;

        const NodeIndex leaf = cursor.GetActiveLeaf();
        if (leaf == InvalidNodeIndex)
        {
            return decision;
        }

        for (const NodeIndex guardIndex : program.m_guardNodes)
        {
            AZ_Assert(guardIndex < program.m_nodes.size(), "A guard index must address a node in the program");

            const DecisionNode& guard = program.m_nodes[guardIndex];
            AZ_Assert(guard.m_abort != AbortMode::None,
                "Only a node with an abort mode is stored as a guard, because nothing else needs observing");

            const bool holds = EvaluateNodePredicate(guard, context);

            // A condition is a leaf, so what it guards is the branch it sits in: its parent's
            // subtree. Pre-order indices encode left to right order, so a running leaf inside
            // that range is protected, and anything past its end is lower priority.
            const NodeIndex owner = guard.m_parent != InvalidNodeIndex ? guard.m_parent : guardIndex;
            AZ_Assert(owner < program.m_nodes.size(), "A guard's owning branch must be a node in the program");

            const NodeIndex ownerEnd = program.m_nodes[owner].m_subtreeEnd;
            AZ_Assert(ownerEnd > owner, "A subtree must end after the node that owns it");
            const bool leafIsInside = leaf >= owner && leaf < ownerEnd;

            if (!holds && leafIsInside &&
                (guard.m_abort == AbortMode::Self || guard.m_abort == AbortMode::Both))
            {
                decision.m_action = AbortAction::Fail;
                decision.m_node = guardIndex;
                return decision;
            }

            if (holds && !leafIsInside && leaf >= ownerEnd &&
                (guard.m_abort == AbortMode::LowerPriority || guard.m_abort == AbortMode::Both))
            {
                // Re-enter at the guarded branch, not at the condition, so the whole branch runs.
                // The highest priority guard wins, and guards are stored in pre-order.
                decision.m_action = AbortAction::Restart;
                decision.m_node = owner;
                return decision;
            }
        }

        return decision;
    }
} // namespace GOAT
