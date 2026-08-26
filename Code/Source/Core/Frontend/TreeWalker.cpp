#include <Core/Frontend/TreeWalker.h>

#include <Core/Frontend/DirectBackend.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

namespace GOAT
{
    namespace
    {
        //! Builds a step reporting that the tree finished.
        //! Written out rather than brace initialised because AZ::EntityId's default constructor is explicit.
        WalkStep Finished(ActionResult result)
        {
            WalkStep step;
            step.m_outcome = WalkOutcome::Finished;
            step.m_result = result;
            return step;
        }

        //! Builds a step carrying an intent for a backend.
        WalkStep Emitted(Intent intent)
        {
            WalkStep step;
            step.m_outcome = WalkOutcome::Intent;
            step.m_intent = AZStd::move(intent);
            step.m_result = ActionResult::Running;
            return step;
        }

        //! Index of a composite's nth child, reached by following each earlier sibling's subtree end.
        NodeIndex NthChild(const DecisionProgram& program, NodeIndex parent, AZ::u16 n)
        {
            const DecisionNode& node = program.m_nodes[parent];
            if (n >= node.m_childCount)
            {
                return InvalidNodeIndex;
            }

            NodeIndex child = node.m_firstChild;
            for (AZ::u16 i = 0; i < n && child != InvalidNodeIndex; ++i)
            {
                child = program.m_nodes[child].m_subtreeEnd;
            }
            return child;
        }

        //! Compares two slots of the same type for equality.
        bool SlotsAreEqual(const IBlackboardSystem& blackboard, BlackboardKey left, BlackboardKey right, AgentId agent)
        {
            if (left.GetType() != right.GetType())
            {
                return false;
            }

            switch (left.GetType())
            {
            case BlackboardType::Bool:
            {
                const bool* a = blackboard.Find<bool>(left, agent);
                const bool* b = blackboard.Find<bool>(right, agent);
                return a != nullptr && b != nullptr && *a == *b;
            }
            case BlackboardType::Int:
            {
                const AZ::s64* a = blackboard.Find<AZ::s64>(left, agent);
                const AZ::s64* b = blackboard.Find<AZ::s64>(right, agent);
                return a != nullptr && b != nullptr && *a == *b;
            }
            case BlackboardType::Float:
            {
                const float* a = blackboard.Find<float>(left, agent);
                const float* b = blackboard.Find<float>(right, agent);
                return a != nullptr && b != nullptr && *a == *b;
            }
            case BlackboardType::EntityId:
            {
                const AZ::EntityId* a = blackboard.Find<AZ::EntityId>(left, agent);
                const AZ::EntityId* b = blackboard.Find<AZ::EntityId>(right, agent);
                return a != nullptr && b != nullptr && *a == *b;
            }
            case BlackboardType::Name:
            {
                const AZ::Name* a = blackboard.Find<AZ::Name>(left, agent);
                const AZ::Name* b = blackboard.Find<AZ::Name>(right, agent);
                return a != nullptr && b != nullptr && *a == *b;
            }
            default:
                return false;
            }
        }
    } // namespace

    bool TreeWalker::EvaluatePredicate(const DecisionNode& node, const PlanContext& context) const
    {
        if (context.m_blackboard == nullptr || !node.m_key.IsValid())
        {
            return false;
        }

        if (node.m_op == NodeOp::Compare)
        {
            return SlotsAreEqual(*context.m_blackboard, node.m_key, node.m_otherKey, context.m_agent);
        }

        const bool* value = context.m_blackboard->Find<bool>(node.m_key, context.m_agent);
        return value != nullptr && *value;
    }

    Intent TreeWalker::MakeIntent(const DecisionNode& node, NodeIndex index) const
    {
        Intent intent;
        intent.m_node = index;

        switch (node.m_op)
        {
        case NodeOp::Action:
            intent.m_backend = DirectBackend::GetBackendName();
            intent.m_direct = node.m_action;
            break;
        case NodeOp::Script:
            intent.m_backend = DirectBackend::GetBackendName();
            intent.m_direct.m_action = CoreActions::RunScript;
            intent.m_direct.m_tag = node.m_tag;
            break;
        case NodeOp::Delegate:
            intent.m_backend = node.m_tag;
            intent.m_goal = node.m_goal;
            break;
        default:
            break;
        }

        return intent;
    }

    WalkStep TreeWalker::Begin(
        const DecisionProgram& program, DecisionCursor& cursor, const PlanContext& context) const
    {
        if (program.IsEmpty())
        {
            return Finished(ActionResult::Failure);
        }

        cursor.SetActiveLeaf(InvalidNodeIndex);
        return Run(program, cursor, context, 0, false, ActionResult::Success);
    }

    WalkStep TreeWalker::Advance(
        const DecisionProgram& program, DecisionCursor& cursor, const PlanContext& context, ActionResult lastResult) const
    {
        const NodeIndex leaf = cursor.GetActiveLeaf();
        if (leaf == InvalidNodeIndex)
        {
            return Begin(program, cursor, context);
        }

        cursor.SetActiveLeaf(InvalidNodeIndex);
        return Run(program, cursor, context, leaf, true, lastResult);
    }

    WalkStep TreeWalker::Run(
        const DecisionProgram& program,
        DecisionCursor& cursor,
        const PlanContext& context,
        NodeIndex node,
        bool bubbling,
        ActionResult result) const
    {
        while (node != InvalidNodeIndex)
        {
            if (!bubbling)
            {
                const DecisionNode& current = program.m_nodes[node];
                switch (current.m_op)
                {
                case NodeOp::Selector:
                case NodeOp::Sequence:
                    cursor.ChildIndex(node) = 0;
                    node = current.m_firstChild;
                    continue;

                case NodeOp::Condition:
                case NodeOp::Compare:
                    if (!EvaluatePredicate(current, context))
                    {
                        result = ActionResult::Failure;
                        bubbling = true;
                        continue;
                    }
                    node = current.m_firstChild;
                    continue;

                case NodeOp::Invert:
                case NodeOp::ForceSuccess:
                    node = current.m_firstChild;
                    continue;

                case NodeOp::Cooldown:
                    // Still cooling down, so the guarded subtree is not entered at all.
                    if (cursor.Deadline(node) > cursor.GetNow())
                    {
                        result = ActionResult::Failure;
                        bubbling = true;
                        continue;
                    }
                    node = current.m_firstChild;
                    continue;

                case NodeOp::Loop:
                    cursor.Counter(node) = 0;
                    node = current.m_firstChild;
                    continue;

                case NodeOp::ConditionalLoop:
                    if (!EvaluatePredicate(current, context))
                    {
                        result = ActionResult::Success;
                        bubbling = true;
                        continue;
                    }
                    node = current.m_firstChild;
                    continue;

                case NodeOp::TimeLimit:
                    cursor.Deadline(node) = cursor.GetNow() + current.m_amount;
                    node = current.m_firstChild;
                    continue;

                case NodeOp::Action:
                case NodeOp::Script:
                case NodeOp::Delegate:
                    cursor.SetActiveLeaf(node);
                    return Emitted(MakeIntent(current, node));

                default:
                    // A node type with no walker support fails rather than stalling the agent.
                    result = ActionResult::Failure;
                    bubbling = true;
                    continue;
                }
            }

            const NodeIndex parentIndex = program.m_nodes[node].m_parent;
            if (parentIndex == InvalidNodeIndex)
            {
                return Finished(result);
            }

            const DecisionNode& parent = program.m_nodes[parentIndex];
            switch (parent.m_op)
            {
            case NodeOp::Selector:
            {
                if (result == ActionResult::Success)
                {
                    node = parentIndex;
                    continue;
                }

                AZ::u16& childIndex = cursor.ChildIndex(parentIndex);
                ++childIndex;
                if (childIndex < parent.m_childCount)
                {
                    node = NthChild(program, parentIndex, childIndex);
                    bubbling = false;
                    continue;
                }

                node = parentIndex;
                result = ActionResult::Failure;
                continue;
            }

            case NodeOp::Sequence:
            {
                if (result == ActionResult::Failure)
                {
                    node = parentIndex;
                    continue;
                }

                AZ::u16& childIndex = cursor.ChildIndex(parentIndex);
                ++childIndex;
                if (childIndex < parent.m_childCount)
                {
                    node = NthChild(program, parentIndex, childIndex);
                    bubbling = false;
                    continue;
                }

                node = parentIndex;
                result = ActionResult::Success;
                continue;
            }

            case NodeOp::Invert:
                result = result == ActionResult::Success ? ActionResult::Failure : ActionResult::Success;
                node = parentIndex;
                continue;

            case NodeOp::ForceSuccess:
                result = ActionResult::Success;
                node = parentIndex;
                continue;

            case NodeOp::Cooldown:
                // The cooldown starts when the subtree finishes, not when it was entered.
                cursor.Deadline(parentIndex) = cursor.GetNow() + parent.m_amount;
                node = parentIndex;
                continue;

            case NodeOp::Loop:
            {
                AZ::u16& count = cursor.Counter(parentIndex);
                ++count;
                const AZ::u16 limit = static_cast<AZ::u16>(parent.m_amount);
                if (result == ActionResult::Success && count < limit)
                {
                    node = parent.m_firstChild;
                    bubbling = false;
                    continue;
                }
                node = parentIndex;
                continue;
            }

            case NodeOp::ConditionalLoop:
                if (result == ActionResult::Success && EvaluatePredicate(parent, context))
                {
                    node = parent.m_firstChild;
                    bubbling = false;
                    continue;
                }
                node = parentIndex;
                continue;

            case NodeOp::TimeLimit:
                if (cursor.GetNow() > cursor.Deadline(parentIndex))
                {
                    result = ActionResult::Failure;
                }
                node = parentIndex;
                continue;

            default:
                node = parentIndex;
                continue;
            }
        }

        return Finished(result);
    }
} // namespace GOAT
