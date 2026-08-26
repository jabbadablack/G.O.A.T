#include <GOAT/Domain/BlackboardStorage.h>

#include <AzCore/std/containers/array.h>

namespace GOAT
{
    void BlackboardStorage::EnsureCapacity(const BlackboardLayout& layout)
    {
        const auto count = [&layout](BlackboardType type)
        {
            return layout.m_slotCounts[static_cast<size_t>(type)];
        };

        // Remember what was already there so only the new slots get seeded.
        AZStd::array<size_t, static_cast<size_t>(BlackboardType::Count)> previous{};
        previous[static_cast<size_t>(BlackboardType::Bool)] = m_bools.size();
        previous[static_cast<size_t>(BlackboardType::Int)] = m_ints.size();
        previous[static_cast<size_t>(BlackboardType::Float)] = m_floats.size();
        previous[static_cast<size_t>(BlackboardType::Vector3)] = m_vectors.size();
        previous[static_cast<size_t>(BlackboardType::EntityId)] = m_entities.size();
        previous[static_cast<size_t>(BlackboardType::Name)] = m_names.size();
        previous[static_cast<size_t>(BlackboardType::Quaternion)] = m_quaternions.size();
        previous[static_cast<size_t>(BlackboardType::Transform)] = m_transforms.size();
        previous[static_cast<size_t>(BlackboardType::EntityIdList)] = m_entityLists.size();

        m_bools.resize(count(BlackboardType::Bool), false);
        m_ints.resize(count(BlackboardType::Int), 0);
        m_floats.resize(count(BlackboardType::Float), 0.0f);
        m_vectors.resize(count(BlackboardType::Vector3), AZ::Vector3::CreateZero());
        m_entities.resize(count(BlackboardType::EntityId), AZ::EntityId{});
        m_names.resize(count(BlackboardType::Name), AZ::Name{});
        m_quaternions.resize(count(BlackboardType::Quaternion), AZ::Quaternion::CreateIdentity());
        m_transforms.resize(count(BlackboardType::Transform), AZ::Transform::CreateIdentity());
        m_entityLists.resize(count(BlackboardType::EntityIdList), EntityIdList{});

        for (const auto& [key, value] : layout.m_defaults)
        {
            AZ_Assert(key.IsValid(), "A declared default must carry a valid key");

            // Only new slots get seeded: an existing agent must keep whatever it already holds.
            if (key.GetIndex() >= previous[static_cast<size_t>(key.GetType())])
            {
                ApplyDefault(key, value);
            }
        }

        AZ_Assert(m_bools.size() == count(BlackboardType::Bool), "Storage must be sized to the layout it was grown to");
        AZ_Assert(m_floats.size() == count(BlackboardType::Float), "Storage must be sized to the layout it was grown to");
    }

    void BlackboardStorage::Reset(const BlackboardLayout& layout)
    {
        m_bools.clear();
        m_ints.clear();
        m_floats.clear();
        m_vectors.clear();
        m_entities.clear();
        m_names.clear();
        m_quaternions.clear();
        m_transforms.clear();
        m_entityLists.clear();

        EnsureCapacity(layout);

        AZ_Assert(m_bools.size() == layout.m_slotCounts[static_cast<size_t>(BlackboardType::Bool)],
            "Resetting must leave storage sized exactly to the layout");
    }

    void BlackboardStorage::ApplyDefault(BlackboardKey key, const AZStd::any& value)
    {
        AZ_Assert(key.IsValid(), "A default is only applied through a valid key");
        AZ_Assert(!value.empty(), "An empty default would overwrite a slot with nothing");

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
