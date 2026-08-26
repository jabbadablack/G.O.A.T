#include <GOAT/Assets/BlackboardAsset.h>

#include <AzCore/Name/Name.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    namespace
    {
        //! Shows a field when the variable is of the matching type, otherwise hides it.
        AZ::Crc32 VisibleWhen(bool matches)
        {
            return matches ? AZ::Edit::PropertyVisibility::Show : AZ::Edit::PropertyVisibility::Hide;
        }
    } // namespace

    AZ::Crc32 BlackboardVariable::GetBoolVisibility() const
    {
        return VisibleWhen(m_type == BlackboardType::Bool);
    }

    AZ::Crc32 BlackboardVariable::GetIntVisibility() const
    {
        return VisibleWhen(m_type == BlackboardType::Int);
    }

    AZ::Crc32 BlackboardVariable::GetFloatVisibility() const
    {
        return VisibleWhen(m_type == BlackboardType::Float);
    }

    AZ::Crc32 BlackboardVariable::GetVector3Visibility() const
    {
        return VisibleWhen(m_type == BlackboardType::Vector3);
    }

    AZ::Crc32 BlackboardVariable::GetEntityIdVisibility() const
    {
        return VisibleWhen(m_type == BlackboardType::EntityId);
    }

    AZ::Crc32 BlackboardVariable::GetNameVisibility() const
    {
        return VisibleWhen(m_type == BlackboardType::Name);
    }

    AZStd::any BlackboardVariable::GetDefault() const
    {
        AZ_Assert(m_type < BlackboardType::Count, "A blackboard variable's type is out of range");

        switch (m_type)
        {
        case BlackboardType::Bool:
            return AZStd::any(m_boolDefault);
        case BlackboardType::Int:
            return AZStd::any(m_intDefault);
        case BlackboardType::Float:
            return AZStd::any(m_floatDefault);
        case BlackboardType::Vector3:
            return AZStd::any(m_vector3Default);
        case BlackboardType::EntityId:
            return AZStd::any(m_entityIdDefault);
        case BlackboardType::Name:
            return AZStd::any(AZ::Name(m_nameDefault));
        default:
            // Quaternion, Transform and lists start at identity or empty, which needs no authoring.
            return {};
        }
    }

    void BlackboardVariable::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Class<BlackboardVariable>()
            ->Version(1)
            ->Field("Name", &BlackboardVariable::m_name)
            ->Field("Scope", &BlackboardVariable::m_scope)
            ->Field("Type", &BlackboardVariable::m_type)
            ->Field("BoolDefault", &BlackboardVariable::m_boolDefault)
            ->Field("IntDefault", &BlackboardVariable::m_intDefault)
            ->Field("FloatDefault", &BlackboardVariable::m_floatDefault)
            ->Field("Vector3Default", &BlackboardVariable::m_vector3Default)
            ->Field("EntityIdDefault", &BlackboardVariable::m_entityIdDefault)
            ->Field("NameDefault", &BlackboardVariable::m_nameDefault);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (editContext == nullptr)
        {
            return;
        }

        editContext->Class<BlackboardVariable>("Blackboard Variable", "One named value on a blackboard")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlackboardVariable::m_name, "Name", "Name this variable is referenced by")
            ->DataElement(AZ::Edit::UIHandlers::ComboBox, &BlackboardVariable::m_scope, "Scope", "Which lifetime the variable belongs to")
            ->DataElement(AZ::Edit::UIHandlers::ComboBox, &BlackboardVariable::m_type, "Type", "What kind of value the variable holds")
                ->Attribute(AZ::Edit::Attributes::ChangeNotify, AZ::Edit::PropertyRefreshLevels::EntireTree)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlackboardVariable::m_boolDefault, "Default", "Starting value")
                ->Attribute(AZ::Edit::Attributes::Visibility, &BlackboardVariable::GetBoolVisibility)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlackboardVariable::m_intDefault, "Default", "Starting value")
                ->Attribute(AZ::Edit::Attributes::Visibility, &BlackboardVariable::GetIntVisibility)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlackboardVariable::m_floatDefault, "Default", "Starting value")
                ->Attribute(AZ::Edit::Attributes::Visibility, &BlackboardVariable::GetFloatVisibility)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlackboardVariable::m_vector3Default, "Default", "Starting value")
                ->Attribute(AZ::Edit::Attributes::Visibility, &BlackboardVariable::GetVector3Visibility)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlackboardVariable::m_entityIdDefault, "Default", "Starting value")
                ->Attribute(AZ::Edit::Attributes::Visibility, &BlackboardVariable::GetEntityIdVisibility)
            ->DataElement(AZ::Edit::UIHandlers::Default, &BlackboardVariable::m_nameDefault, "Default", "Starting value")
                ->Attribute(AZ::Edit::Attributes::Visibility, &BlackboardVariable::GetNameVisibility);
    }

    void BlackboardAsset::Reflect(AZ::ReflectContext* context)
    {
        BlackboardVariable::Reflect(context);

        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        // EnableForAssetEditor is what puts this type in the Asset Editor's new asset list.
        serializeContext->Class<BlackboardAsset, AZ::Data::AssetData>()
            ->Version(1)
            ->Attribute(AZ::Edit::Attributes::EnableForAssetEditor, true)
            ->Field("Variables", &BlackboardAsset::m_variables);

        if (AZ::EditContext* editContext = serializeContext->GetEditContext())
        {
            editContext->Class<BlackboardAsset>("Blackboard", "Blackboard variables shared by GOAT agents")
                ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                ->DataElement(
                    AZ::Edit::UIHandlers::Default, &BlackboardAsset::m_variables, "Variables", "Variables this asset declares")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true);
        }
    }
} // namespace GOAT
