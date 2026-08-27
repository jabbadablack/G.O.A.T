#pragma once

#include <AzCore/base.h>
#include <AzCore/std/hash.h>

namespace GOAT
{
    //! Generation checked reference to a slot in a store that owns the thing it addresses.
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

namespace AZStd
{
    template<typename Tag>
    struct hash<GOAT::Handle<Tag>>
    {
        size_t operator()(const GOAT::Handle<Tag>& handle) const
        {
            return (static_cast<size_t>(handle.GetGeneration()) << 32) ^ handle.GetIndex();
        }
    };
} // namespace AZStd
