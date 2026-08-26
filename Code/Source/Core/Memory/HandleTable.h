#pragma once

#include <GOAT/Domain/Handle.h>

#include <AzCore/Casting/numeric_cast.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/utils.h>

namespace GOAT
{
    //! Dense storage addressed by generation checked handles.
    //! Values stay contiguous for iteration, and releasing a slot invalidates any handle to it
    //! instead of letting a stale handle alias the next value stored there.
    template<typename T, typename Tag>
    class HandleTable final
    {
    public:
        using HandleType = Handle<Tag>;

        //! Stores a value and returns a handle to it.
        template<typename... Args>
        HandleType Acquire(Args&&... args)
        {
            AZ::u32 slotIndex;
            if (!m_freeSlots.empty())
            {
                slotIndex = m_freeSlots.back();
                m_freeSlots.pop_back();
            }
            else
            {
                slotIndex = aznumeric_cast<AZ::u32>(m_slots.size());
                m_slots.push_back(Slot{});
            }

            m_dense.emplace_back(AZStd::forward<Args>(args)...);
            m_denseToSlot.push_back(slotIndex);
            m_slots[slotIndex].m_denseIndex = aznumeric_cast<AZ::u32>(m_dense.size() - 1);

            return HandleType(slotIndex, m_slots[slotIndex].m_generation);
        }

        //! Destroys the value a handle refers to. Returns false for a stale handle.
        bool Release(HandleType handle)
        {
            if (!IsValid(handle))
            {
                return false;
            }

            const AZ::u32 slotIndex = handle.GetIndex();
            const AZ::u32 denseIndex = m_slots[slotIndex].m_denseIndex;
            const AZ::u32 lastIndex = aznumeric_cast<AZ::u32>(m_dense.size() - 1);

            // Swap the last value down so the dense array stays contiguous.
            if (denseIndex != lastIndex)
            {
                m_dense[denseIndex] = AZStd::move(m_dense[lastIndex]);
                const AZ::u32 movedSlot = m_denseToSlot[lastIndex];
                m_denseToSlot[denseIndex] = movedSlot;
                m_slots[movedSlot].m_denseIndex = denseIndex;
            }

            m_dense.pop_back();
            m_denseToSlot.pop_back();

            m_slots[slotIndex].m_denseIndex = HandleType::NullIndex;
            ++m_slots[slotIndex].m_generation;
            m_freeSlots.push_back(slotIndex);
            return true;
        }

        //! True when the handle still refers to a live value.
        bool IsValid(HandleType handle) const
        {
            return !handle.IsNull() && handle.GetIndex() < m_slots.size() &&
                m_slots[handle.GetIndex()].m_generation == handle.GetGeneration() &&
                m_slots[handle.GetIndex()].m_denseIndex != HandleType::NullIndex;
        }

        //! Returns the value, or nullptr when the handle is stale.
        T* Find(HandleType handle)
        {
            return IsValid(handle) ? &m_dense[m_slots[handle.GetIndex()].m_denseIndex] : nullptr;
        }

        //! Returns the value, or nullptr when the handle is stale.
        const T* Find(HandleType handle) const
        {
            return IsValid(handle) ? &m_dense[m_slots[handle.GetIndex()].m_denseIndex] : nullptr;
        }

        //! Returns the handle for a value at a dense index, for iterating with handles.
        HandleType GetHandleAt(size_t denseIndex) const
        {
            const AZ::u32 slotIndex = m_denseToSlot[denseIndex];
            return HandleType(slotIndex, m_slots[slotIndex].m_generation);
        }

        //! Live values, contiguous, for cache friendly iteration.
        AZStd::vector<T>& GetValues() { return m_dense; }
        const AZStd::vector<T>& GetValues() const { return m_dense; }

        size_t Size() const { return m_dense.size(); }
        bool IsEmpty() const { return m_dense.empty(); }

        //! Destroys every value and invalidates every outstanding handle.
        void Clear()
        {
            m_dense.clear();
            m_denseToSlot.clear();
            m_freeSlots.clear();
            for (AZ::u32 i = 0; i < m_slots.size(); ++i)
            {
                m_slots[i].m_denseIndex = HandleType::NullIndex;
                ++m_slots[i].m_generation;
                m_freeSlots.push_back(i);
            }
        }

    private:
        //! One addressable slot. Free slots carry NullIndex and a bumped generation.
        struct Slot
        {
            AZ::u32 m_denseIndex = HandleType::NullIndex;
            AZ::u32 m_generation = 1;
        };

        AZStd::vector<T> m_dense;
        AZStd::vector<AZ::u32> m_denseToSlot;
        AZStd::vector<Slot> m_slots;
        AZStd::vector<AZ::u32> m_freeSlots;
    };
} // namespace GOAT
