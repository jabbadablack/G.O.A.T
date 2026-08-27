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
} // namespace GOAT
