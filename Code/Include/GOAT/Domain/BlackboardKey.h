#pragma once

#include <GOAT/Domain/BlackboardTypes.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/std/hash.h>

namespace GOAT
{
    //! Resolved handle to one blackboard slot.
    //! Packs scope, type, and the index into that type's dense array so a lookup is an array index.
    class BlackboardKey final
    {
    public:
        AZ_TYPE_INFO(BlackboardKey, BlackboardKeyTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Bits reserved for the slot index within its type's array.
        static constexpr AZ::u32 IndexBitCount = 26;
        //! Bits reserved for the BlackboardType tag.
        static constexpr AZ::u32 TypeBitCount = 4;
        //! Bits reserved for the BlackboardScope tag.
        static constexpr AZ::u32 ScopeBitCount = 2;
        //! Largest slot index the packed layout can address.
        static constexpr AZ::u32 MaxIndex = (1u << IndexBitCount) - 1;

        BlackboardKey() = default;

        // Defined here rather than in a .cpp because the gem's API target is headers only:
        // a module gem in this tree links no GOAT object, so anything it calls must be inline.
        BlackboardKey(BlackboardScope scope, BlackboardType type, AZ::u32 index)
        {
            AZ_Assert(index <= MaxIndex, "Blackboard slot index %u exceeds the packed limit of %u", index, MaxIndex);
            AZ_Assert(scope < BlackboardScope::Count, "Invalid blackboard scope");
            AZ_Assert(type < BlackboardType::Count, "Invalid blackboard type");

            m_packed = (static_cast<AZ::u32>(scope) << ScopeShift) | (static_cast<AZ::u32>(type) << TypeShift) |
                (index & IndexMask);
        }

        //! Rebuilds a key from what GetPacked returned. Only a value that came from there means
        //! anything; any other answers as a slot that does not exist.
        static BlackboardKey FromPacked(AZ::u32 packed)
        {
            BlackboardKey key;
            key.m_packed = packed;
            return key;
        }

        //! True when this key refers to a real slot.
        bool IsValid() const { return m_packed != InvalidPacked; }

        BlackboardScope GetScope() const { return static_cast<BlackboardScope>((m_packed >> ScopeShift) & ScopeMask); }
        BlackboardType GetType() const { return static_cast<BlackboardType>((m_packed >> TypeShift) & TypeMask); }
        AZ::u32 GetIndex() const { return m_packed & IndexMask; }

        //! The raw packed value, for hashing and serialization.
        AZ::u32 GetPacked() const { return m_packed; }

        bool operator==(const BlackboardKey& rhs) const { return m_packed == rhs.m_packed; }
        bool operator!=(const BlackboardKey& rhs) const { return m_packed != rhs.m_packed; }
        bool operator<(const BlackboardKey& rhs) const { return m_packed < rhs.m_packed; }

    private:
        //! Reserved value meaning "no slot".
        static constexpr AZ::u32 InvalidPacked = 0xFFFFFFFFu;

        static constexpr AZ::u32 IndexMask = (1u << IndexBitCount) - 1;
        static constexpr AZ::u32 TypeMask = (1u << TypeBitCount) - 1;
        static constexpr AZ::u32 ScopeMask = (1u << ScopeBitCount) - 1;
        static constexpr AZ::u32 TypeShift = IndexBitCount;
        static constexpr AZ::u32 ScopeShift = IndexBitCount + TypeBitCount;

        AZ::u32 m_packed = InvalidPacked;
    };

    static_assert(
        BlackboardKey::IndexBitCount + BlackboardKey::TypeBitCount + BlackboardKey::ScopeBitCount == 32,
        "BlackboardKey bit fields must exactly fill a 32 bit word");
    static_assert(
        static_cast<AZ::u32>(BlackboardType::Count) <= (1u << BlackboardKey::TypeBitCount),
        "BlackboardType no longer fits in its packed field");
    static_assert(
        static_cast<AZ::u32>(BlackboardScope::Count) <= (1u << BlackboardKey::ScopeBitCount),
        "BlackboardScope no longer fits in its packed field");
} // namespace GOAT

namespace AZStd
{
    template<>
    struct hash<GOAT::BlackboardKey>
    {
        size_t operator()(const GOAT::BlackboardKey& key) const { return key.GetPacked(); }
    };
} // namespace AZStd
