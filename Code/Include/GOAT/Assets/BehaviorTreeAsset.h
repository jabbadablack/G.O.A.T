#pragma once

#include <GOAT/GOATTypeIds.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Math/Vector2.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/any.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! One authored property on a node, resolved against the node type at compile time.
    struct BehaviorTreeProperty final
    {
        AZ_TYPE_INFO(BehaviorTreeProperty, BehaviorTreePropertyTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_name;
        AZStd::any m_value;
    };

    //! Editor only data a graph tool round trips. The runtime never reads it.
    struct BehaviorTreeNodeMetadata final
    {
        AZ_TYPE_INFO(BehaviorTreeNodeMetadata, BehaviorTreeNodeMetadataTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Where the node sits on a graph canvas.
        AZ::Vector2 m_position = AZ::Vector2::CreateZero();
        //! Author's note about the node.
        AZStd::string m_comment;
    };

    //! One authored node. Children are in execution order, left to right.
    struct BehaviorTreeNode final
    {
        AZ_TYPE_INFO(BehaviorTreeNode, BehaviorTreeNodeTypeId);
        AZ_CLASS_ALLOCATOR(BehaviorTreeNode, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        //! Node type name, resolved against the node type registry when compiled.
        AZStd::string m_type;
        //! Authored properties for this node.
        AZStd::vector<BehaviorTreeProperty> m_properties;
        //! Services attached to this node, valid only on composites.
        AZStd::vector<BehaviorTreeNode> m_services;
        //! Children, in execution order.
        AZStd::vector<BehaviorTreeNode> m_children;
        //! Ignored at runtime.
        BehaviorTreeNodeMetadata m_metadata;
    };

    //! A behavior tree as authored, before it is compiled for execution.
    //! Lua builds one of these in memory; a future graph editor saves one as a .bt file.
    //! The runtime never sees this type, only the DecisionProgram compiled from it.
    class BehaviorTreeAsset final
        : public AZ::Data::AssetData
    {
    public:
        AZ_RTTI(BehaviorTreeAsset, BehaviorTreeAssetTypeId, AZ::Data::AssetData);
        AZ_CLASS_ALLOCATOR(BehaviorTreeAsset, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        //! Source extension a future graph editor saves to.
        static constexpr const char* FileExtension = "bt";
        //! Group this asset is filed under in the Asset Editor.
        static constexpr const char* AssetGroup = "GOAT";
        //! Name shown in the Asset Editor's new asset list.
        static constexpr const char* DisplayName = "Behavior Tree";

        //! Name agents refer to this tree by.
        AZStd::string m_name;
        //! The root of the tree.
        BehaviorTreeNode m_root;
    };
} // namespace GOAT
