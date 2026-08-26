#include <Core/Frontend/GuardEvaluator.h>

#include <Core/Frontend/NodePredicate.h>

namespace GOAT
{
    AbortDecision GuardEvaluator::Evaluate(
        const DecisionProgram& program, const DecisionCursor& cursor, const PlanContext& context) const
    {
        AbortDecision decision;

        const NodeIndex leaf = cursor.GetActiveLeaf();
        if (leaf == InvalidNodeIndex)
        {
            return decision;
        }

        for (const NodeIndex guardIndex : program.m_guardNodes)
        {
            const DecisionNode& guard = program.m_nodes[guardIndex];
            const bool holds = EvaluateNodePredicate(guard, context);

            // A condition is a leaf, so what it guards is the branch it sits in: its parent's
            // subtree. Pre-order indices encode left to right order, so a running leaf inside
            // that range is protected, and anything past its end is lower priority.
            const NodeIndex owner = guard.m_parent != InvalidNodeIndex ? guard.m_parent : guardIndex;
            const NodeIndex ownerEnd = program.m_nodes[owner].m_subtreeEnd;
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
