#include <Core/Frontend/TreeWalker.h>

#include <Core/Frontend/DirectBackend.h>
#include <Core/Frontend/NodePredicate.h>

#include <GOAT/Interfaces/INodeScripting.h>

#include <AzCore/Console/ILogger.h>


namespace GOAT
{
    namespace
    {
        //! Builds a step reporting that the tree finished.
        //! Written out rather than brace initialised because AZ::EntityId's default constructor is explicit.
        WalkStep Finished(ActionResult result, float wakeAt = AZStd::numeric_limits<float>::max())
        {
            WalkStep step;
            step.m_outcome = WalkOutcome::Finished;
            step.m_result = result;
            step.m_wakeAt = wakeAt;
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
            AZ_Assert(parent < program.m_nodes.size(), "A composite index must address a node in the program");
            AZ_Assert(child > parent && child < program.m_nodes[parent].m_subtreeEnd,
                "A child must sit inside its parent's subtree, which pre-order indices encode as a range");

            NodeIndex candidate = program.m_nodes[parent].m_firstChild;
            for (AZ::u16 i = 0; i < program.m_nodes[parent].m_childCount; ++i)
            {
                if (candidate == child)
                {
                    return i;
                }
                candidate = program.m_nodes[candidate].m_subtreeEnd;
            }

            AZ_Assert(false, "A descendant of a composite must be reachable through one of its children");
            return 0;
        }

        //! Index of a composite's nth child, reached by following each earlier sibling's subtree end.
        NodeIndex NthChild(const DecisionProgram& program, NodeIndex parent, AZ::u16 n)
        {
            AZ_Assert(parent < program.m_nodes.size(), "A composite index must address a node in the program");

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
        AZ_Assert(node.m_op == NodeOp::Action || node.m_op == NodeOp::Script || node.m_op == NodeOp::Delegate,
            "Only an action, script or delegate leaf emits an intent");

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

        AZ_Assert(!intent.m_backend.IsEmpty(), "Every intent names the backend that must satisfy it");
        return intent;
    }

    WalkStep TreeWalker::Begin(
        const DecisionProgram& program, DecisionCursor& cursor, const PlanContext& context) const
    {
        AZ_Assert(!program.IsEmpty(), "A walk only ever begins on a compiled program");
        if (program.IsEmpty())
        {
            AZ_Error("GOAT", false, "An agent cannot start: its decision program is empty");
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

        AZ_Assert(leaf < program.m_nodes.size(), "The active leaf must address a node in the program");
        cursor.SetActiveLeaf(InvalidNodeIndex);
        return Run(program, cursor, context, leaf, true, lastResult);
    }

    WalkStep TreeWalker::Restart(
        const DecisionProgram& program, DecisionCursor& cursor, const PlanContext& context, NodeIndex node) const
    {
        // Point every composite above the node at the branch that reaches it, so the walk
        // resumes there instead of where the abandoned branch left off.
        AZ_Assert(node < program.m_nodes.size(), "A restart target must address a node in the program");

        NodeIndex child = node;
        NodeIndex parent = program.m_nodes[node].m_parent;
        while (parent != InvalidNodeIndex)
        {
            // Only a Lua composite remembers which child it chose; a built in one finds its
            // next sibling from the child it is leaving, so it has nothing to rebuild.
            if (program.m_nodes[parent].m_op == NodeOp::LuaComposite)
            {
                cursor.Slot(program.m_nodes[parent].m_cursorSlot) = static_cast<float>(ChildIndexOf(program, parent, child));
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
        AZ_Assert(node == InvalidNodeIndex || node < program.m_nodes.size(),
            "A walk only ever starts at a node in the program");

        // The soonest a cooldown turned this walk away. Carried out with the finished step so an
        // agent that found no work knows whether waiting could ever change that.
        float wakeAt = AZStd::numeric_limits<float>::max();

        while (node != InvalidNodeIndex)
        {
            AZ_Assert(node < program.m_nodes.size(), "The walk stepped outside the program");

            if (!bubbling)
            {
                const DecisionNode& current = program.m_nodes[node];
                switch (current.m_op)
                {
                case NodeOp::Selector:
                case NodeOp::Sequence:
                    node = current.m_firstChild;
                    continue;

                case NodeOp::Condition:
                case NodeOp::Compare:
                    // A leaf: evaluate and report straight back to the parent composite.
                    result = EvaluateNodePredicate(current, context) ? ActionResult::Success : ActionResult::Failure;
                    bubbling = true;
                    continue;

                case NodeOp::Parallel:
                    // Only the main branch is ever walked. The background one is a predicate
                    // that GuardEvaluator re-checks; stepping into it would need a second
                    // action slot, and an agent's state machine has exactly one.
                    node = current.m_firstChild;
                    continue;

                case NodeOp::Invert:
                case NodeOp::ForceSuccess:
                    node = current.m_firstChild;
                    continue;

                case NodeOp::Cooldown:
                    // Still cooling down, so the guarded subtree is not entered at all.
                    if (cursor.GetSlot(current.m_cursorSlot) > cursor.GetNow())
                    {
                        wakeAt = AZStd::min(wakeAt, cursor.GetSlot(current.m_cursorSlot));
                        result = ActionResult::Failure;
                        bubbling = true;
                        continue;
                    }
                    node = current.m_firstChild;
                    continue;

                case NodeOp::Loop:
                    cursor.Slot(current.m_cursorSlot) = 0.0f;
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
                    cursor.Slot(current.m_cursorSlot) = cursor.GetNow() + current.m_amount;
                    node = current.m_firstChild;
                    continue;

                case NodeOp::LuaComposite:
                {
                    // The user's own control flow chooses which child runs first.
                    if (context.m_scripting == nullptr)
                    {
                        AZ_Error("GOAT", false,
                            "Node %u is a Lua composite but scripting is not available, so its branch fails", node);
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

                    cursor.Slot(current.m_cursorSlot) = static_cast<float>(child);
                    node = NthChild(program, node, static_cast<AZ::u16>(child));
                    AZ_Assert(node != InvalidNodeIndex, "A child index inside the child count must resolve to a node");
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
                    AZ_Error("GOAT", false, "Node %u has op %u, which the walker does not implement",
                        node, static_cast<AZ::u32>(current.m_op));
                    result = ActionResult::Failure;
                    bubbling = true;
                    continue;
                }
            }

            const NodeIndex parentIndex = program.m_nodes[node].m_parent;
            if (parentIndex == InvalidNodeIndex)
            {
                return Finished(result, wakeAt);
            }

            AZ_Assert(parentIndex < node, "A parent always precedes its children in pre-order");

            const DecisionNode& parent = program.m_nodes[parentIndex];
            AZ_Assert(parent.m_childCount > 0, "A node that has a child cannot report a zero child count");

            switch (parent.m_op)
            {
            case NodeOp::Selector:
            {
                if (result == ActionResult::Success)
                {
                    node = parentIndex;
                    continue;
                }

                // The next sibling begins where this child's subtree ends, so stepping to it is
                // one load. Counting children instead meant walking the sibling chain from the
                // first one every time, which made a wide composite cost the square of its width.
                const NodeIndex next = program.m_nodes[node].m_subtreeEnd;
                if (next < parent.m_subtreeEnd)
                {
                    node = next;
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

                // The next sibling begins where this child's subtree ends, so stepping to it is
                // one load. Counting children instead meant walking the sibling chain from the
                // first one every time, which made a wide composite cost the square of its width.
                const NodeIndex next = program.m_nodes[node].m_subtreeEnd;
                if (next < parent.m_subtreeEnd)
                {
                    node = next;
                    bubbling = false;
                    continue;
                }

                node = parentIndex;
                result = ActionResult::Success;
                continue;
            }

            case NodeOp::Parallel:
                // The main branch finishing finishes the parallel, carrying its result up.
                AZ_Assert(parent.m_childCount == 2, "A compiled parallel always has two branches");
                node = parentIndex;
                continue;

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
                cursor.Slot(parent.m_cursorSlot) = cursor.GetNow() + parent.m_amount;
                node = parentIndex;
                continue;

            case NodeOp::Loop:
            {
                const AZ::u16 count = static_cast<AZ::u16>(cursor.GetSlot(parent.m_cursorSlot)) + 1;
                cursor.Slot(parent.m_cursorSlot) = static_cast<float>(count);

                const AZ::u16 limit = static_cast<AZ::u16>(parent.m_amount);
                AZ_Warning("GOAT", limit > 0, "Node %u loops zero times, so its child runs exactly once", parentIndex);
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
                if (cursor.GetNow() > cursor.GetSlot(parent.m_cursorSlot))
                {
                    result = ActionResult::Failure;
                }
                node = parentIndex;
                continue;

            case NodeOp::LuaComposite:
            {
                if (context.m_scripting == nullptr)
                {
                    AZ_Error("GOAT", false,
                        "Node %u is a Lua composite but scripting is not available, so its result passes through",
                        parentIndex);
                    node = parentIndex;
                    continue;
                }

                ActionResult finished = result;
                const int child = context.m_scripting->AdvanceComposite(
                    parent.m_tag, context, parentIndex,
                    static_cast<AZ::u16>(cursor.GetSlot(parent.m_cursorSlot)), result, finished);

                if (child < 0 || child >= parent.m_childCount)
                {
                    result = finished;
                    node = parentIndex;
                    continue;
                }

                cursor.Slot(parent.m_cursorSlot) = static_cast<float>(child);
                node = NthChild(program, parentIndex, static_cast<AZ::u16>(child));
                AZ_Assert(node != InvalidNodeIndex, "A child index inside the child count must resolve to a node");
                bubbling = false;
                continue;
            }

            case NodeOp::LuaDecorator:
                AZ_Error("GOAT", context.m_scripting != nullptr,
                    "Node %u is a Lua decorator but scripting is not available, so its result passes through",
                    parentIndex);
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

        return Finished(result, wakeAt);
    }
} // namespace GOAT
