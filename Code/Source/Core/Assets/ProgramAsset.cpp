#include <GOAT/Assets/ProgramAsset.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void AuthoredProperty::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AuthoredProperty>()
                ->Version(1)
                ->Field("Name", &AuthoredProperty::m_name)
                ->Field("Value", &AuthoredProperty::m_value);
        }
    }

    void AuthoredNodeMetadata::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AuthoredNodeMetadata>()
                ->Version(1)
                ->Field("Position", &AuthoredNodeMetadata::m_position)
                ->Field("Comment", &AuthoredNodeMetadata::m_comment);
        }
    }

    void AuthoredNode::Reflect(AZ::ReflectContext* context)
    {
        AuthoredProperty::Reflect(context);
        AuthoredNodeMetadata::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<AuthoredNode>()
                ->Version(1)
                ->Field("Type", &AuthoredNode::m_type)
                ->Field("Properties", &AuthoredNode::m_properties)
                ->Field("Services", &AuthoredNode::m_services)
                ->Field("Children", &AuthoredNode::m_children)
                ->Field("Metadata", &AuthoredNode::m_metadata);
        }
    }

    void ProgramAsset::Reflect(AZ::ReflectContext* context)
    {
        AuthoredNode::Reflect(context);

        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        // Deliberately not EnableForAssetEditor: a program is made in the asset browser and
        // authored on the canvas, not filled in as a property grid.
        serializeContext->Class<ProgramAsset, AZ::Data::AssetData>()
            ->Version(1)
            ->Field("Name", &ProgramAsset::m_name)
            ->Field("Root", &ProgramAsset::m_root);
    }
} // namespace GOAT
