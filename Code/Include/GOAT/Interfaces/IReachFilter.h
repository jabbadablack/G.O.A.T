#pragma once

#include <GOAT/Domain/AgentId.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>

namespace GOAT
{
    //! Narrows a director's reach by something the core cannot judge.
    //!
    //! The core knows an agent's entity, tree, squad and straight line distance, and deliberately
    //! knows nothing about paths or facing. A module registers one of these under a name and a
    //! director names it, which is how navigation aware reach lives in the navigation gem while
    //! the core stays genre neutral.
    class IReachFilter
    {
    public:
        AZ_RTTI(IReachFilter, IReachFilterTypeId);

        virtual ~IReachFilter() = default;

        //! Name a director's filter field selects this by.
        virtual AZ::Name GetName() const = 0;

        //! True when an agent is in reach of a director.
        //!
        //! Both parties are named, not just their positions: a filter may care about something
        //! only one of them has -- which way the director is facing, what the agent is doing --
        //! and neither is derivable from a point.
        //!
        //! Called once per candidate that survived the cheaper filters, on the director's own
        //! tick, so it must answer without blocking.
        virtual bool IsInReach(
            AgentId director,
            const AZ::Vector3& directorPosition,
            AgentId agent,
            const AZ::Vector3& agentPosition,
            float range) const = 0;
    };
} // namespace GOAT
