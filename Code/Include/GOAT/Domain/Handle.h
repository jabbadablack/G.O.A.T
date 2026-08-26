#pragma once

#include <AzCore/base.h>

namespace GOAT
{
    //! Generation checked reference to a slot in a HandleTable.
    //! The Tag parameter keeps handles of different kinds from being assigned to each other.
    template<typename Tag>
    class Handle final
    {
    public:
        //! Slot index meaning "refers to nothing".
        static constexpr AZ::u32 NullIndex = static_cast<AZ::u32>(-1);

        Handle() = default;
        Handle(AZ::u32 index, AZ::u32 generation)
            : m_index(index)
            , m_generation(generation)
        {
        }

        //! True when this handle was never pointed at a slot.
        bool IsNull() const { return m_index == NullIndex; }

        AZ::u32 GetIndex() const { return m_index; }
        AZ::u32 GetGeneration() const { return m_generation; }

        bool operator==(const Handle& rhs) const
        {
            return m_index == rhs.m_index && m_generation == rhs.m_generation;
        }
        bool operator!=(const Handle& rhs) const { return !(*this == rhs); }

    private:
        AZ::u32 m_index = NullIndex;
        AZ::u32 m_generation = 0;
    };
} // namespace GOAT
