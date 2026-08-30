#pragma once

#include <GOAT/Domain/BlackboardLayout.h>

#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/Domain/BlackboardTraits.h>

#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Dense typed values for one blackboard scope instance.
    //! Each type gets its own array, so reading a slot is an array index with no type branch.
    class BlackboardStorage final
    {
    public:
        //! Signalled with the key that changed, so observers wake only for what they watch.
        //! How many times anything in this storage has actually changed. An agent compares it
        //! against the count it last acted on, which is what replaces subscribing to a change.
        AZ::u32 GetEpoch() const { return m_epoch; }

        //! Grows every array to the layout's slot counts, seeding only the newly added slots.
        //! Existing values are kept, so declaring a variable later does not disturb live agents.
        void EnsureCapacity(const BlackboardLayout& layout);

        //! Discards every value and re-seeds from the layout.
        void Reset(const BlackboardLayout& layout);

        //! Returns the value at a key, or nullptr when the key is the wrong type or out of range.
        template<typename T>
        const T* Find(BlackboardKey key) const;

        //! Writes a value. Returns false when the key is the wrong type or out of range.
        //! Writing the value a slot already holds does not signal observers.
        template<typename T>
        bool Set(BlackboardKey key, const T& value);

        //! Subscribes a handler to every change in this storage.

    private:
        //! Returns the array that holds a given value type.
        template<typename T>
        AZStd::vector<T>& Array();
        template<typename T>
        const AZStd::vector<T>& Array() const;

        //! Writes a default held in an any, if it holds the type the key expects.
        void ApplyDefault(BlackboardKey key, const AZStd::any& value);

        AZStd::vector<bool> m_bools;
        AZStd::vector<AZ::s64> m_ints;
        AZStd::vector<float> m_floats;
        AZStd::vector<AZ::Vector3> m_vectors;
        AZStd::vector<AZ::EntityId> m_entities;
        AZStd::vector<AZ::Name> m_names;
        AZStd::vector<AZ::Quaternion> m_quaternions;
        AZStd::vector<AZ::Transform> m_transforms;
        AZStd::vector<EntityIdList> m_entityLists;

        // Per-slot epochs for each value array to allow detecting changes to individual keys.
        AZStd::vector<AZ::u32> m_boolsEpochs;
        AZStd::vector<AZ::u32> m_intsEpochs;
        AZStd::vector<AZ::u32> m_floatsEpochs;
        AZStd::vector<AZ::u32> m_vectorsEpochs;
        AZStd::vector<AZ::u32> m_entitiesEpochs;
        AZStd::vector<AZ::u32> m_namesEpochs;
        AZStd::vector<AZ::u32> m_quaternionsEpochs;
        AZStd::vector<AZ::u32> m_transformsEpochs;
        AZStd::vector<AZ::u32> m_entityListsEpochs;

        //! Starts at one so a watcher's zeroed count never matches an untouched storage.
        AZ::u32 m_epoch = 1;

        //! Returns the epoch counter for a specific key, or zero when the key is invalid or out of range.
        AZ::u32 GetKeyEpoch(BlackboardKey key) const;
    };

// Binds each value type to the array that stores it.
#define GOAT_BLACKBOARD_ARRAY(TYPE, MEMBER)                                                                            \
    template<>                                                                                                         \
    inline AZStd::vector<TYPE>& BlackboardStorage::Array<TYPE>()                                                       \
    {                                                                                                                  \
        return MEMBER;                                                                                                 \
    }                                                                                                                  \
    template<>                                                                                                         \
    inline const AZStd::vector<TYPE>& BlackboardStorage::Array<TYPE>() const                                           \
    {                                                                                                                  \
        return MEMBER;                                                                                                 \
    }

    GOAT_BLACKBOARD_ARRAY(bool, m_bools)
    GOAT_BLACKBOARD_ARRAY(AZ::s64, m_ints)
    GOAT_BLACKBOARD_ARRAY(float, m_floats)
    GOAT_BLACKBOARD_ARRAY(AZ::Vector3, m_vectors)
    GOAT_BLACKBOARD_ARRAY(AZ::EntityId, m_entities)
    GOAT_BLACKBOARD_ARRAY(AZ::Name, m_names)
    GOAT_BLACKBOARD_ARRAY(AZ::Quaternion, m_quaternions)
    GOAT_BLACKBOARD_ARRAY(AZ::Transform, m_transforms)
    GOAT_BLACKBOARD_ARRAY(EntityIdList, m_entityLists)

#undef GOAT_BLACKBOARD_ARRAY

    template<typename T>
    const T* BlackboardStorage::Find(BlackboardKey key) const
    {
        if (!key.IsValid() || key.GetType() != BlackboardTypeOf<T>::Value)
        {
            return nullptr;
        }

        const AZStd::vector<T>& values = Array<T>();
        return key.GetIndex() < values.size() ? &values[key.GetIndex()] : nullptr;
    }

    template<typename T>
    bool BlackboardStorage::Set(BlackboardKey key, const T& value)
    {
        if (!key.IsValid() || key.GetType() != BlackboardTypeOf<T>::Value)
        {
            return false;
        }

        AZStd::vector<T>& values = Array<T>();
        if (key.GetIndex() >= values.size())
        {
            return false;
        }

        T& slot = values[key.GetIndex()];
        if (slot == value)
        {
            return true;
        }

        slot = value;

        // Only a real change counts. A write of the value already there must not wake anybody,
        // which is what keeps a director writing the same order every tick from costing anything.
        ++m_epoch;

        // Also update the per-key epoch for finer-grained watches.
        const AZ::u32 idx = key.GetIndex();
        switch (key.GetType())
        {
        case BlackboardType::Bool:
            if (idx < m_boolsEpochs.size()) ++m_boolsEpochs[idx];
            break;
        case BlackboardType::Int:
            if (idx < m_intsEpochs.size()) ++m_intsEpochs[idx];
            break;
        case BlackboardType::Float:
            if (idx < m_floatsEpochs.size()) ++m_floatsEpochs[idx];
            break;
        case BlackboardType::Vector3:
            if (idx < m_vectorsEpochs.size()) ++m_vectorsEpochs[idx];
            break;
        case BlackboardType::EntityId:
            if (idx < m_entitiesEpochs.size()) ++m_entitiesEpochs[idx];
            break;
        case BlackboardType::Name:
            if (idx < m_namesEpochs.size()) ++m_namesEpochs[idx];
            break;
        case BlackboardType::Quaternion:
            if (idx < m_quaternionsEpochs.size()) ++m_quaternionsEpochs[idx];
            break;
        case BlackboardType::Transform:
            if (idx < m_transformsEpochs.size()) ++m_transformsEpochs[idx];
            break;
        case BlackboardType::EntityIdList:
            if (idx < m_entityListsEpochs.size()) ++m_entityListsEpochs[idx];
            break;
        default:
            break;
        }

        return true;
    }
} // namespace GOAT
