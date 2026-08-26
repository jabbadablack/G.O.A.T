#pragma once

#include <Core/Domain/BlackboardLayout.h>

#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/Domain/BlackboardTraits.h>

#include <AzCore/EBus/Event.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Dense typed values for one blackboard scope instance.
    //! Each type gets its own array, so reading a slot is an array index with no type branch.
    class BlackboardStorage final
    {
    public:
        //! Signalled with the key that changed, so observers wake only for what they watch.
        using ChangedEvent = AZ::Event<BlackboardKey>;

        //! Sizes every array from a layout and applies its declared defaults.
        void Reset(const BlackboardLayout& layout);

        //! Returns the value at a key, or nullptr when the key is the wrong type or out of range.
        template<typename T>
        const T* Find(BlackboardKey key) const;

        //! Writes a value. Returns false when the key is the wrong type or out of range.
        //! Writing the value a slot already holds does not signal observers.
        template<typename T>
        bool Set(BlackboardKey key, const T& value);

        //! Subscribes a handler to every change in this storage.
        void ConnectChangedHandler(ChangedEvent::Handler& handler) { handler.Connect(m_changed); }

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

        ChangedEvent m_changed;
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
        m_changed.Signal(key);
        return true;
    }
} // namespace GOAT
