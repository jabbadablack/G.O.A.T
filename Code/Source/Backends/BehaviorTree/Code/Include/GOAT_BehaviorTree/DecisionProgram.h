#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/AgentProgram.h>
#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT_BehaviorTree/Guard.h>
#include <GOAT/Domain/AgentDebug.h>
#include <GOAT/Domain/Intent.h>
#include <GOAT/Domain/NodeType.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Deepest tree the walker will run, which bounds an agent's cursor.
    inline constexpr size_t MaxTreeDepth = 32;

    //! How much per node state one tree may need an agent to carry. Only a node whose state has
    //! to survive between ticks takes a slot -- a cooldown's expiry, a loop's count, a time
    //! limit, a Lua composite's chosen child -- plus one per service. Authored trees use a
    //! handful, so this is headroom rather than a budget, and the compiler names any tree that
    //! exceeds it rather than letting an agent carry state it cannot address.
    inline constexpr AZ::u16 MaxCursorSlots = 16;

    //! A node that carries no state between ticks.
    inline constexpr AZ::u16 InvalidCursorSlot = static_cast<AZ::u16>(-1);

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
        //! Primary numeric property: cooldown seconds, loop count, time limit, or duration.
        float m_amount = 0.0f;
        //! How close counts as arrived, kept apart from m_amount so a node may carry both.
        float m_tolerance = 0.0f;
        //! Where this node's state lives in an agent's cursor, or InvalidCursorSlot when it
        //! keeps none. Assigned by the compiler, so an agent carries one value per node that
        //! needs one rather than one per node in the tree.
        AZ::u16 m_cursorSlot = InvalidCursorSlot;
        //! First service attached to this node, indexing the program's service table.
        AZ::u32 m_firstService = 0;
        //! How many services are attached to this node.
        AZ::u16 m_serviceCount = 0;
    };

    //! A behavior tree compiled for execution: flat, immutable, and shared by every agent using it.
    class DecisionProgram final
        : public AgentProgram
    {
    public:
        AZ_RTTI(DecisionProgram, DecisionProgramTypeId, AgentProgram);
        AZ_CLASS_ALLOCATOR(DecisionProgram, AZ::SystemAllocator);

        //! True when the program has no nodes to run.
        bool IsEmpty() const { return m_nodes.empty(); }

        //! Nodes in pre-order, root first.
        AZStd::vector<DecisionNode> m_nodes;
        //! Which authored node each compiled node came from, indexed the same as m_nodes.
        //! A side table rather than a field on DecisionNode, so the node stays the size the
        //! walker wants, the way m_guardNodes and m_serviceNodes already are. It carries a
        //! tree name because an inlined subtree's nodes were authored somewhere else.
        AZStd::vector<ProgramNodeRef> m_authored;
        //! Services referenced by node service ranges.
        AZStd::vector<DecisionService> m_services;
        //! Every blackboard slot a guard in this tree observes, deduplicated.
        //! Registering observers only for these is what keeps evaluation event driven.
        AZStd::vector<BlackboardKey> m_observedKeys;
        //! Nodes that declared an abort mode, so re-checking scans only guards.
        AZStd::vector<NodeIndex> m_guardNodes;
        //! Nodes that carry services, so due checks scan only those.
        AZStd::vector<NodeIndex> m_serviceNodes;
        //! Parallel nodes, so re-checking their background branches scans only those.
        AZStd::vector<NodeIndex> m_parallelNodes;
        //! Cursor slots this tree needs, and where the run of one slot per service starts.
        AZ::u16 m_cursorSlotCount = 0;
        AZ::u16 m_serviceSlotBase = 0;
        //! Deepest path in this tree, checked against MaxTreeDepth at compile time.
        AZ::u32 m_depth = 0;
    };
} // namespace GOAT
