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
    struct AuthoredProperty final
    {
        AZ_TYPE_INFO(AuthoredProperty, AuthoredPropertyTypeId);

        static void Reflect(AZ::ReflectContext* context);

        AZStd::string m_name;
        AZStd::any m_value;
    };

    //! Editor only data a graph tool round trips. The runtime never reads it.
    struct AuthoredNodeMetadata final
    {
        AZ_TYPE_INFO(AuthoredNodeMetadata, AuthoredNodeMetadataTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Where the node sits on a graph canvas.
        AZ::Vector2 m_position = AZ::Vector2::CreateZero();
        //! Author's note about the node.
        AZStd::string m_comment;
    };

    //! One authored node. Children are in execution order, left to right.
    struct AuthoredNode final
    {
        AZ_TYPE_INFO(AuthoredNode, AuthoredNodeTypeId);
        AZ_CLASS_ALLOCATOR(AuthoredNode, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        //! Node type name, resolved against the node type registry when compiled.
        AZStd::string m_type;
        //! Authored properties for this node.
        AZStd::vector<AuthoredProperty> m_properties;
        //! Services attached to this node, valid only on composites.
        AZStd::vector<AuthoredNode> m_services;
        //! Children, in execution order.
        AZStd::vector<AuthoredNode> m_children;
        //! Ignored at runtime.
        AuthoredNodeMetadata m_metadata;
    };

    //! How many nodes hang below this one, counting services and children alike.
    inline size_t BelowCount(const AuthoredNode& node)
    {
        return node.m_services.size() + node.m_children.size();
    }

    //! The node one step of a path names, or null when the path leads nowhere.
    //! Services come first, then children, sharing one index space. Defined once because the
    //! validator, the canvas and every compiler have to mean the same node by "the third one"
    //! or they each point somewhere different.
    inline const AuthoredNode* StepInto(const AuthoredNode& node, size_t index)
    {
        if (index < node.m_services.size())
        {
            return &node.m_services[index];
        }

        const size_t child = index - node.m_services.size();
        return child < node.m_children.size() ? &node.m_children[child] : nullptr;
    }

    //! A program as authored, before it is compiled for execution.
    //! Lua builds one of these in memory; the graph editor saves one as a .goat file.
    //! The runtime never sees this type, only the program a backend compiled from it.
    class ProgramAsset final
        : public AZ::Data::AssetData
    {
    public:
        AZ_RTTI(ProgramAsset, ProgramAssetTypeId, AZ::Data::AssetData);
        AZ_CLASS_ALLOCATOR(ProgramAsset, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        //! Source extension the graph editor saves to.
        static constexpr const char* FileExtension = "goat";
        //! Group this asset is filed under in the Asset Editor.
        static constexpr const char* AssetGroup = "GOAT";
        //! Name shown in the Asset Editor's new asset list.
        static constexpr const char* DisplayName = "GOAT Program";

        //! Name agents refer to this program by.
        AZStd::string m_name;
        //! The root of the tree.
        AuthoredNode m_root;
    };
} // namespace GOAT
