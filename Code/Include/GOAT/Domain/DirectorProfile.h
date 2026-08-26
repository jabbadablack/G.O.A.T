#pragma once

#include <GOAT/GOATTypeIds.h>

#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>

namespace GOAT
{
    //! What an agent switching its own tree carries. Every director outranks it, which is the
    //! intended default: a director exists to overrule what an agent decided for itself.
    inline constexpr AZ::u8 SelfSwitchPriority = 0;

    //! Which agents one director governs.
    //!
    //! The filters that are set combine with AND; an unset one is no constraint, so a director
    //! with none governs every agent. Narrowing rather than widening is what an author expects
    //! from a field labelled "Squad", and it is what makes "squad Alpha, within 30 m, running
    //! Patrol" expressible at all. Two directors give you the OR, and priority settles the overlap.
    struct DirectorReach final
    {
        AZ_TYPE_INFO(DirectorReach, DirectorReachTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Governs only this squad. Empty for any.
        AZ::Name m_squad;
        //! Governs only agents currently running this tree. Empty for any.
        AZ::Name m_tree;
        //! Governs only agents this close. Zero for any distance.
        float m_radius = 0.0f;
        //! A registered reach filter to narrow by. Empty for plain straight line distance.
        //! Named rather than typed so that navigation aware reach can live in the navigation
        //! gem and the core can stay ignorant of it.
        AZ::Name m_filter;
    };

    //! What one director governs, and how forcefully.
    struct DirectorProfile final
    {
        AZ_TYPE_INFO(DirectorProfile, DirectorProfileTypeId);

        static void Reflect(AZ::ReflectContext* context);

        DirectorReach m_reach;

        //! Higher outranks lower when two directors command the same agent in one window.
        //! Above SelfSwitchPriority, so any director outranks an agent switching itself.
        AZ::u8 m_priority = 1;

        //! How long before this director may command the same agent the same way again.
        //! Held per director rather than per agent, so one director's order cannot silence another.
        float m_cooldownSeconds = 5.0f;
    };
} // namespace GOAT
