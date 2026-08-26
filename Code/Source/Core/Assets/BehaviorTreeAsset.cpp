#include <GOAT/Assets/BehaviorTreeAsset.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void BehaviorTreeProperty::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BehaviorTreeProperty>()
                ->Version(1)
                ->Field("Name", &BehaviorTreeProperty::m_name)
                ->Field("Value", &BehaviorTreeProperty::m_value);
        }
    }

    void BehaviorTreeNodeMetadata::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BehaviorTreeNodeMetadata>()
                ->Version(1)
                ->Field("Position", &BehaviorTreeNodeMetadata::m_position)
                ->Field("Comment", &BehaviorTreeNodeMetadata::m_comment);
        }
    }

    void BehaviorTreeNode::Reflect(AZ::ReflectContext* context)
    {
        BehaviorTreeProperty::Reflect(context);
        BehaviorTreeNodeMetadata::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BehaviorTreeNode>()
                ->Version(1)
                ->Field("Type", &BehaviorTreeNode::m_type)
                ->Field("Properties", &BehaviorTreeNode::m_properties)
                ->Field("Services", &BehaviorTreeNode::m_services)
                ->Field("Children", &BehaviorTreeNode::m_children)
                ->Field("Metadata", &BehaviorTreeNode::m_metadata);
        }
    }

    void BehaviorTreeAsset::Reflect(AZ::ReflectContext* context)
    {
        BehaviorTreeNode::Reflect(context);

        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Class<BehaviorTreeAsset, AZ::Data::AssetData>()
            ->Version(1)
            ->Attribute(AZ::Edit::Attributes::EnableForAssetEditor, true)
            ->Field("Name", &BehaviorTreeAsset::m_name)
            ->Field("Root", &BehaviorTreeAsset::m_root);
    }
} // namespace GOAT
