#pragma once

#include <GOAT/Domain/BlackboardTypes.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! What a node does structurally, which decides how many children it may have.
    enum class NodeKind : AZ::u8
    {
        Composite, //!< Any number of children.
        Decorator, //!< Exactly one child.
        Leaf,      //!< No children.
        Service    //!< Attached to a composite and ticked on an interval.
    };

    //! What a compiled node does when the walker reaches it.
    //! Closed on purpose: extension node types run through the Lua ops.
    enum class NodeOp : AZ::u8
    {
        Selector,        //!< Runs children until one succeeds.
        Sequence,        //!< Runs children until one fails.
        Parallel,        //!< Runs one main child alongside a background child.
        Invert,          //!< Flips its child's success and failure.
        ForceSuccess,    //!< Reports success whatever its child does.
        Cooldown,        //!< Blocks re-entry until a duration has passed.
        Loop,            //!< Repeats its child a fixed number of times.
        ConditionalLoop, //!< Repeats its child while a condition holds.
        TimeLimit,       //!< Fails its child once a duration elapses.
        Condition,       //!< Guards a subtree on a blackboard value.
        Compare,         //!< Guards a subtree on two blackboard values.
        Action,          //!< Emits an inline action for the direct backend.
        Script,          //!< Runs a Lua behavior.
        Delegate,        //!< Hands an intent to a named backend.
        Subtree,         //!< Runs another compiled tree.
        LuaComposite,    //!< Composite whose control flow is written in Lua.
        LuaDecorator,    //!< Decorator whose control flow is written in Lua.
        Count
    };

    //! One authored parameter a node type accepts.
    //! Drives authoring validation now and a graph editor's property panel later.
    struct NodeParameter final
    {
        AZ_TYPE_INFO(NodeParameter, NodeParameterTypeId);

        //! Property name as authored.
        AZ::Name m_name;
        //! What kind of value the property holds.
        BlackboardType m_type = BlackboardType::Float;
        //! The value names a blackboard variable rather than being a literal.
        bool m_isBlackboardKey = false;
        //! Authoring fails when this property is missing.
        bool m_required = false;
    };

    //! Everything the authoring layers need to know about one node type.
    struct NodeTypeDescriptor final
    {
        AZ_TYPE_INFO(NodeTypeDescriptor, NodeTypeDescriptorTypeId);

        //! Name this node type is written as.
        AZ::Name m_name;
        //! How many children it may have.
        NodeKind m_kind = NodeKind::Leaf;
        //! What the walker does with it.
        NodeOp m_op = NodeOp::Action;
        //! Palette grouping for a future graph editor.
        AZStd::string m_category;
        //! One line describing what the node is for.
        AZStd::string m_description;
        //! Properties this node type accepts.
        AZStd::vector<NodeParameter> m_parameters;
    };

    //! Reflects the node type enums for serialization and scripting.
    void ReflectNodeTypes(AZ::ReflectContext* context);
} // namespace GOAT

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(GOAT::NodeKind, "{9C1DD16E-3B37-4AA0-99C1-A11C6A5D0BBE}");
    AZ_TYPE_INFO_SPECIALIZE(GOAT::NodeOp, "{6E9F8B48-DA96-4E64-BF41-DF5AB1E1D2A4}");
} // namespace AZ
