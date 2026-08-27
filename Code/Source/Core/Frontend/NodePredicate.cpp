#include <Core/Frontend/NodePredicate.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

namespace GOAT
{
    namespace
    {
        //! Compares two slots of the same type for equality.
        bool SlotsAreEqual(const IBlackboardSystem& blackboard, BlackboardKey left, BlackboardKey right, AgentId agent)
        {
            AZ_Assert(left.IsValid() && right.IsValid(), "A compare always names two declared variables");
            AZ_Warning("GOAT", left.GetType() == right.GetType(),
                "A compare reads variables of different types, so it can never match");

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

    bool EvaluateNodePredicate(const DecisionNode& node, const PlanContext& context)
    {
        AZ_Assert(node.m_op == NodeOp::Condition || node.m_op == NodeOp::Compare,
            "Only a condition or a compare has a predicate to evaluate");
        AZ_Assert(context.m_blackboard != nullptr, "Evaluating a predicate needs a blackboard");
        AZ_Assert(node.m_key.IsValid(), "A compiled predicate always names a declared variable");

        if (context.m_blackboard == nullptr || !node.m_key.IsValid())
        {
            return false;
        }

        if (node.m_op == NodeOp::Compare)
        {
            return SlotsAreEqual(*context.m_blackboard, node.m_key, node.m_otherKey, context.m_agent);
        }

        AZ_Assert(node.m_key.GetType() == BlackboardType::Bool, "A condition reads a boolean variable");

        const bool* value = context.m_blackboard->Find<bool>(node.m_key, context.m_agent);
        AZ_Warning("GOAT", value != nullptr,
            "A condition reads a variable agent %u has no storage for, so it always fails", context.m_agent.GetIndex());
        return value != nullptr && *value;
    }
    ActionResult EvaluateSubtree(const DecisionProgram& program, NodeIndex index, const PlanContext& context)
    {
        AZ_Assert(index < program.m_nodes.size(), "A background branch must address a node in the program");
        if (index >= program.m_nodes.size())
        {
            return ActionResult::Failure;
        }

        const DecisionNode& node = program.m_nodes[index];
        switch (node.m_op)
        {
        case NodeOp::Condition:
        case NodeOp::Compare:
            return EvaluateNodePredicate(node, context) ? ActionResult::Success : ActionResult::Failure;

        case NodeOp::Invert:
        {
            AZ_Assert(node.m_firstChild != InvalidNodeIndex, "A decorator always has its one child");
            const ActionResult child = EvaluateSubtree(program, node.m_firstChild, context);
            return child == ActionResult::Success ? ActionResult::Failure : ActionResult::Success;
        }

        case NodeOp::ForceSuccess:
            AZ_Assert(node.m_firstChild != InvalidNodeIndex, "A decorator always has its one child");
            EvaluateSubtree(program, node.m_firstChild, context);
            return ActionResult::Success;

        case NodeOp::Selector:
        {
            // Succeeds on the first child that does, like the walker's own selector.
            NodeIndex child = node.m_firstChild;
            for (AZ::u16 i = 0; i < node.m_childCount; ++i)
            {
                AZ_Assert(child != InvalidNodeIndex, "A composite's child count must match its children");
                if (EvaluateSubtree(program, child, context) == ActionResult::Success)
                {
                    return ActionResult::Success;
                }
                child = program.m_nodes[child].m_subtreeEnd;
            }
            return ActionResult::Failure;
        }

        case NodeOp::Sequence:
        {
            NodeIndex child = node.m_firstChild;
            for (AZ::u16 i = 0; i < node.m_childCount; ++i)
            {
                AZ_Assert(child != InvalidNodeIndex, "A composite's child count must match its children");
                if (EvaluateSubtree(program, child, context) == ActionResult::Failure)
                {
                    return ActionResult::Failure;
                }
                child = program.m_nodes[child].m_subtreeEnd;
            }
            return ActionResult::Success;
        }

        default:
            // The compiler rejects everything else in a background branch, so reaching this
            // means the two disagree about what "instantaneous" means.
            AZ_Assert(false, "Node %u has op %u, which cannot appear in a parallel's background branch",
                index, static_cast<AZ::u32>(node.m_op));
            return ActionResult::Failure;
        }
    }
} // namespace GOAT
