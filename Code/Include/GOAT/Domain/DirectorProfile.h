#pragma once

#include <GOAT/GOATTypeIds.h>

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>

namespace GOAT
{
    //! What an agent switching its own tree carries. Every director outranks it, which is the
    //! intended default: a director exists to overrule what an agent decided for itself.
    inline constexpr AZ::u8 SelfSwitchPriority = 0;

    //! What one director governs with, and how forcefully.
    //!
    //! Not who it governs: a director reaches every agent until a filter component beside it
    //! narrows that. Several filters combine with AND, two directors give you the OR, and
    //! priority settles the overlap.
    struct DirectorProfile final
    {
        AZ_TYPE_INFO(DirectorProfile, DirectorProfileTypeId);

        //! Higher outranks lower when two directors command the same agent in one window.
        //! Above SelfSwitchPriority, so any director outranks an agent switching itself.
        AZ::u8 m_priority = 1;

        //! How long before this director may command the same agent the same way again.
        //! Held per director rather than per agent, so one director's order cannot silence another.
        float m_cooldownSeconds = 5.0f;
    };
} // namespace GOAT
