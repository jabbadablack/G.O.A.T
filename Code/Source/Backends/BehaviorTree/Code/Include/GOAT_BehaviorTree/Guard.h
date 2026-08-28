#pragma once

#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/Domain/Intent.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/RTTI/TypeInfo.h>

namespace GOAT
{
    //! What a tripped guard interrupts. Mirrors Unreal's observer abort modes.
    enum class AbortMode : AZ::u8
    {
        None,          //!< Never interrupts.
        Self,          //!< Aborts this node and any subtree running under it.
        LowerPriority, //!< Aborts any node to the right of this one.
        Both           //!< Aborts both of the above.
    };

    //! A condition watched while a plan runs, re-checked only when its key changes.
    struct Guard final
    {
        AZ_TYPE_INFO(Guard, GuardTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Blackboard slot this guard observes.
        BlackboardKey m_key;
        //! Tree node that declared the guard, used to decide what the abort affects.
        NodeIndex m_node = InvalidNodeIndex;
        //! What to interrupt when the condition stops holding.
        AbortMode m_abort = AbortMode::None;
    };

    //! Reflects the guard types for serialization and scripting.
    void ReflectGuardTypes(AZ::ReflectContext* context);
} // namespace GOAT

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(GOAT::AbortMode, "{AE2755BD-3E8D-4B6E-B197-8881B9F57978}");
} // namespace AZ
