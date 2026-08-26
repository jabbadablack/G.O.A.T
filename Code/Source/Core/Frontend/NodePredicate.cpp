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
} // namespace GOAT
