#include <GOAT/Domain/BlackboardKey.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    namespace
    {
        constexpr AZ::u32 IndexMask = (1u << BlackboardKey::IndexBitCount) - 1;
        constexpr AZ::u32 TypeMask = (1u << BlackboardKey::TypeBitCount) - 1;
        constexpr AZ::u32 ScopeMask = (1u << BlackboardKey::ScopeBitCount) - 1;
        constexpr AZ::u32 TypeShift = BlackboardKey::IndexBitCount;
        constexpr AZ::u32 ScopeShift = BlackboardKey::IndexBitCount + BlackboardKey::TypeBitCount;
    } // namespace

    void BlackboardKey::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<BlackboardKey>()->Version(1)->Field("Packed", &BlackboardKey::m_packed);
        }
    }

    BlackboardKey::BlackboardKey(BlackboardScope scope, BlackboardType type, AZ::u32 index)
    {
        AZ_Assert(index <= MaxIndex, "Blackboard slot index %u exceeds the packed limit of %u", index, MaxIndex);
        AZ_Assert(scope < BlackboardScope::Count, "Invalid blackboard scope");
        AZ_Assert(type < BlackboardType::Count, "Invalid blackboard type");

        m_packed = (static_cast<AZ::u32>(scope) << ScopeShift) | (static_cast<AZ::u32>(type) << TypeShift) |
            (index & IndexMask);
    }

    bool BlackboardKey::IsValid() const
    {
        return m_packed != InvalidPacked;
    }

    BlackboardScope BlackboardKey::GetScope() const
    {
        return static_cast<BlackboardScope>((m_packed >> ScopeShift) & ScopeMask);
    }

    BlackboardType BlackboardKey::GetType() const
    {
        return static_cast<BlackboardType>((m_packed >> TypeShift) & TypeMask);
    }

    AZ::u32 BlackboardKey::GetIndex() const
    {
        return m_packed & IndexMask;
    }
} // namespace GOAT
