#include <GOAT/Domain/BlackboardTypes.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    void ReflectBlackboardTypes(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Enum<BlackboardScope>()
            ->Value("Global", BlackboardScope::Global)
            ->Value("Agent", BlackboardScope::Agent)
            ->Value("Squad", BlackboardScope::Squad);

        serializeContext->Enum<BlackboardType>()
            ->Value("Bool", BlackboardType::Bool)
            ->Value("Int", BlackboardType::Int)
            ->Value("Float", BlackboardType::Float)
            ->Value("Vector3", BlackboardType::Vector3)
            ->Value("EntityId", BlackboardType::EntityId)
            ->Value("Name", BlackboardType::Name)
            ->Value("Quaternion", BlackboardType::Quaternion)
            ->Value("Transform", BlackboardType::Transform)
            ->Value("EntityIdList", BlackboardType::EntityIdList);

        // The serialize context registration above only handles saving and loading.
        // A property editor combo box is populated from the edit context, so the values
        // have to be declared there as well or the drop down comes up empty.
        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (editContext == nullptr)
        {
            return;
        }

        editContext->Enum<BlackboardScope>("Scope", "Which lifetime the variable belongs to")
            ->Value("Global", BlackboardScope::Global)
            ->Value("Agent", BlackboardScope::Agent)
            ->Value("Squad", BlackboardScope::Squad);

        editContext->Enum<BlackboardType>("Type", "What kind of value the variable holds")
            ->Value("Bool", BlackboardType::Bool)
            ->Value("Int", BlackboardType::Int)
            ->Value("Float", BlackboardType::Float)
            ->Value("Vector3", BlackboardType::Vector3)
            ->Value("EntityId", BlackboardType::EntityId)
            ->Value("Name", BlackboardType::Name)
            ->Value("Quaternion", BlackboardType::Quaternion)
            ->Value("Transform", BlackboardType::Transform)
            ->Value("EntityIdList", BlackboardType::EntityIdList);
    }

    AZ::TypeId ToTypeId(BlackboardType type)
    {
        switch (type)
        {
        case BlackboardType::Bool:
            return azrtti_typeid<bool>();
        case BlackboardType::Int:
            return azrtti_typeid<AZ::s64>();
        case BlackboardType::Float:
            return azrtti_typeid<float>();
        case BlackboardType::Vector3:
            return azrtti_typeid<AZ::Vector3>();
        case BlackboardType::EntityId:
            return azrtti_typeid<AZ::EntityId>();
        case BlackboardType::Name:
            return azrtti_typeid<AZ::Name>();
        case BlackboardType::Quaternion:
            return azrtti_typeid<AZ::Quaternion>();
        case BlackboardType::Transform:
            return azrtti_typeid<AZ::Transform>();
        case BlackboardType::EntityIdList:
            return azrtti_typeid<AZStd::vector<AZ::EntityId>>();
        default:
            return AZ::TypeId::CreateNull();
        }
    }

    BlackboardType FromTypeId(const AZ::TypeId& typeId)
    {
        for (AZ::u8 i = 0; i < static_cast<AZ::u8>(BlackboardType::Count); ++i)
        {
            const auto candidate = static_cast<BlackboardType>(i);
            if (ToTypeId(candidate) == typeId)
            {
                return candidate;
            }
        }
        return BlackboardType::Count;
    }

    const char* ToString(BlackboardType type)
    {
        switch (type)
        {
        case BlackboardType::Bool:
            return "Bool";
        case BlackboardType::Int:
            return "Int";
        case BlackboardType::Float:
            return "Float";
        case BlackboardType::Vector3:
            return "Vector3";
        case BlackboardType::EntityId:
            return "EntityId";
        case BlackboardType::Name:
            return "Name";
        case BlackboardType::Quaternion:
            return "Quaternion";
        case BlackboardType::Transform:
            return "Transform";
        case BlackboardType::EntityIdList:
            return "EntityIdList";
        default:
            return "<invalid>";
        }
    }

    const char* ToString(BlackboardScope scope)
    {
        switch (scope)
        {
        case BlackboardScope::Global:
            return "Global";
        case BlackboardScope::Agent:
            return "Agent";
        case BlackboardScope::Squad:
            return "Squad";
        default:
            return "<invalid>";
        }
    }
} // namespace GOAT
