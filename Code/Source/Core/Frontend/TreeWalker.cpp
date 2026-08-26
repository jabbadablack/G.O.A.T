#include <Core/Frontend/TreeWalker.h>

#include <Core/Frontend/DirectBackend.h>
#include <Core/Frontend/NodePredicate.h>

#include <GOAT/Interfaces/INodeScripting.h>


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

        //! Which child of a composite leads to a given descendant.
        AZ::u16 ChildIndexOf(const DecisionProgram& program, NodeIndex parent, NodeIndex child)
        {
            NodeIndex candidate = program.m_nodes[parent].m_firstChild;
            for (AZ::u16 i = 0; i < program.m_nodes[parent].m_childCount; ++i)
            {
                if (candidate == child)
                {
                    return i;
                }
                candidate = program.m_nodes[candidate].m_subtreeEnd;
            }
            return 0;
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

    } // namespace

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

    WalkStep TreeWalker::Restart(
        const DecisionProgram& program, DecisionCursor& cursor, const PlanContext& context, NodeIndex node) const
    {
        // Point every composite above the node at the branch that reaches it, so the walk
        // resumes there instead of where the abandoned branch left off.
        NodeIndex child = node;
        NodeIndex parent = program.m_nodes[node].m_parent;
        while (parent != InvalidNodeIndex)
        {
            const NodeOp op = program.m_nodes[parent].m_op;
            if (op == NodeOp::Selector || op == NodeOp::Sequence)
            {
                cursor.ChildIndex(parent) = ChildIndexOf(program, parent, child);
            }
            child = parent;
            parent = program.m_nodes[parent].m_parent;
        }

        cursor.SetActiveLeaf(InvalidNodeIndex);
        return Run(program, cursor, context, node, false, ActionResult::Success);
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
                    if (!EvaluateNodePredicate(current, context))
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
                    if (!EvaluateNodePredicate(current, context))
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

                case NodeOp::LuaComposite:
                {
                    // The user's own control flow chooses which child runs first.
                    if (context.m_scripting == nullptr)
                    {
                        result = ActionResult::Failure;
                        bubbling = true;
                        continue;
                    }

                    ActionResult finished = ActionResult::Failure;
                    const int child = context.m_scripting->BeginComposite(
                        current.m_tag, context, node, current.m_childCount, finished);
                    if (child < 0 || child >= current.m_childCount)
                    {
                        result = finished;
                        bubbling = true;
                        continue;
                    }

                    cursor.ChildIndex(node) = static_cast<AZ::u16>(child);
                    node = NthChild(program, node, static_cast<AZ::u16>(child));
                    continue;
                }

                case NodeOp::LuaDecorator:
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
                if (result == ActionResult::Success && EvaluateNodePredicate(parent, context))
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

            case NodeOp::LuaComposite:
            {
                if (context.m_scripting == nullptr)
                {
                    node = parentIndex;
                    continue;
                }

                ActionResult finished = result;
                const int child = context.m_scripting->AdvanceComposite(
                    parent.m_tag, context, parentIndex, cursor.ChildIndex(parentIndex), result, finished);

                if (child < 0 || child >= parent.m_childCount)
                {
                    result = finished;
                    node = parentIndex;
                    continue;
                }

                cursor.ChildIndex(parentIndex) = static_cast<AZ::u16>(child);
                node = NthChild(program, parentIndex, static_cast<AZ::u16>(child));
                bubbling = false;
                continue;
            }

            case NodeOp::LuaDecorator:
                if (context.m_scripting != nullptr)
                {
                    result = context.m_scripting->FilterDecorator(parent.m_tag, context, parentIndex, result);
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
