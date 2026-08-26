#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/Domain/Guard.h>
#include <GOAT/Domain/Intent.h>
#include <GOAT/Domain/NodeType.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Deepest tree the walker will run, which bounds an agent's cursor.
    inline constexpr size_t MaxTreeDepth = 32;

    //! A service attached to a composite, ticked on its interval while that subtree is active.
    struct DecisionService final
    {
        AZ_TYPE_INFO(DecisionService, DecisionServiceTypeId);

        //! Lua behavior this service runs.
        AZ::Name m_behavior;
        //! Seconds between ticks.
        float m_interval = 0.5f;
    };

    //! One node of a compiled tree.
    //! Nodes are stored in pre-order, so a node's whole subtree is the range [index, m_subtreeEnd).
    //! Direct children are not adjacent to each other: the next sibling begins at the previous
    //! sibling's m_subtreeEnd, which is how the walker steps between them in constant time.
    struct DecisionNode final
    {
        AZ_TYPE_INFO(DecisionNode, DecisionNodeTypeId);

        //! What the walker does with this node.
        NodeOp m_op = NodeOp::Selector;
        //! What a tripped guard on this node interrupts.
        AbortMode m_abort = AbortMode::None;
        //! Parent node, or InvalidNodeIndex at the root.
        NodeIndex m_parent = InvalidNodeIndex;
        //! First child, which always sits immediately after this node.
        //! Reach later siblings by following each child's m_subtreeEnd.
        NodeIndex m_firstChild = InvalidNodeIndex;
        //! One past the last node in this node's subtree, so skipping it is a jump
        //! and the next sibling starts here.
        NodeIndex m_subtreeEnd = InvalidNodeIndex;
        //! How many children this node has.
        AZ::u16 m_childCount = 0;
        //! Blackboard slot this node reads, for conditions and comparisons.
        BlackboardKey m_key;
        //! Second slot, for comparisons.
        BlackboardKey m_otherKey;
        //! Name this node references: a Lua behavior, a backend, another tree.
        AZ::Name m_tag;
        //! Secondary name, such as the goal handed to a backend.
        AZ::Name m_goal;
        //! Inline action emitted by an Action leaf.
        ActionRequest m_action;
        //! Numeric property: cooldown seconds, loop count, or time limit.
        float m_amount = 0.0f;
        //! First service attached to this node, indexing the program's service table.
        AZ::u32 m_firstService = 0;
        //! How many services are attached to this node.
        AZ::u16 m_serviceCount = 0;
    };

    //! A behavior tree compiled for execution: flat, immutable, and shared by every agent using it.
    class DecisionProgram final
    {
    public:
        AZ_TYPE_INFO(DecisionProgram, DecisionProgramTypeId);

        //! True when the program has no nodes to run.
        bool IsEmpty() const { return m_nodes.empty(); }

        //! Name agents refer to this tree by.
        AZ::Name m_name;
        //! Nodes in pre-order, root first.
        AZStd::vector<DecisionNode> m_nodes;
        //! Services referenced by node service ranges.
        AZStd::vector<DecisionService> m_services;
        //! Every blackboard slot a guard in this tree observes, deduplicated.
        //! Registering observers only for these is what keeps evaluation event driven.
        AZStd::vector<BlackboardKey> m_observedKeys;
        //! Nodes that declared an abort mode, so re-checking scans only guards.
        AZStd::vector<NodeIndex> m_guardNodes;
        //! Deepest path in this tree, checked against MaxTreeDepth at compile time.
        AZ::u32 m_depth = 0;
    };
} // namespace GOAT
