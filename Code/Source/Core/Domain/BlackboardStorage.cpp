#include <Core/Domain/BlackboardStorage.h>

namespace GOAT
{
    void BlackboardStorage::Reset(const BlackboardLayout& layout)
    {
        const auto count = [&layout](BlackboardType type)
        {
            return layout.m_slotCounts[static_cast<size_t>(type)];
        };

        m_bools.assign(count(BlackboardType::Bool), false);
        m_ints.assign(count(BlackboardType::Int), 0);
        m_floats.assign(count(BlackboardType::Float), 0.0f);
        m_vectors.assign(count(BlackboardType::Vector3), AZ::Vector3::CreateZero());
        m_entities.assign(count(BlackboardType::EntityId), AZ::EntityId{});
        m_names.assign(count(BlackboardType::Name), AZ::Name{});
        m_quaternions.assign(count(BlackboardType::Quaternion), AZ::Quaternion::CreateIdentity());
        m_transforms.assign(count(BlackboardType::Transform), AZ::Transform::CreateIdentity());
        m_entityLists.assign(count(BlackboardType::EntityIdList), EntityIdList{});

        for (const auto& [key, value] : layout.m_defaults)
        {
            ApplyDefault(key, value);
        }
    }

    void BlackboardStorage::ApplyDefault(BlackboardKey key, const AZStd::any& value)
    {
// Writes the default when the any holds the type the slot expects.
#define GOAT_APPLY_DEFAULT(TYPE)                                                                                       \
    if (const auto* typed = AZStd::any_cast<TYPE>(&value))                                                             \
    {                                                                                                                  \
        Set<TYPE>(key, *typed);                                                                                        \
        return;                                                                                                        \
    }                                                                                                                  \
    break;

        switch (key.GetType())
        {
        case BlackboardType::Bool:
            GOAT_APPLY_DEFAULT(bool)
        case BlackboardType::Int:
            GOAT_APPLY_DEFAULT(AZ::s64)
        case BlackboardType::Float:
            GOAT_APPLY_DEFAULT(float)
        case BlackboardType::Vector3:
            GOAT_APPLY_DEFAULT(AZ::Vector3)
        case BlackboardType::EntityId:
            GOAT_APPLY_DEFAULT(AZ::EntityId)
        case BlackboardType::Name:
            GOAT_APPLY_DEFAULT(AZ::Name)
        case BlackboardType::Quaternion:
            GOAT_APPLY_DEFAULT(AZ::Quaternion)
        case BlackboardType::Transform:
            GOAT_APPLY_DEFAULT(AZ::Transform)
        case BlackboardType::EntityIdList:
            GOAT_APPLY_DEFAULT(EntityIdList)
        default:
            break;
        }

#undef GOAT_APPLY_DEFAULT

        AZ_Warning(
            "GOAT", false, "Blackboard default for a %s slot holds the wrong type and was ignored",
            ToString(key.GetType()));
    }
} // namespace GOAT
