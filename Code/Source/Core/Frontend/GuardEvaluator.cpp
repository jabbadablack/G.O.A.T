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

            // Pre-order indices encode left to right order, so a leaf inside the guard's
            // range is what the guard protects, and anything past its end is lower priority.
            const bool leafIsInside = leaf >= guardIndex && leaf < guard.m_subtreeEnd;

            if (!holds && leafIsInside &&
                (guard.m_abort == AbortMode::Self || guard.m_abort == AbortMode::Both))
            {
                decision.m_action = AbortAction::Fail;
                decision.m_node = guardIndex;
                return decision;
            }

            if (holds && !leafIsInside && leaf >= guard.m_subtreeEnd &&
                (guard.m_abort == AbortMode::LowerPriority || guard.m_abort == AbortMode::Both))
            {
                // The highest priority guard wins, and guards are stored in pre-order.
                decision.m_action = AbortAction::Restart;
                decision.m_node = guardIndex;
                return decision;
            }
        }

        return decision;
    }
} // namespace GOAT
