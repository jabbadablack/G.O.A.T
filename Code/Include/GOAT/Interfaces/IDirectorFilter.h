#pragma once

#include <GOAT/Domain/AgentId.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/RTTI/RTTI.h>

namespace GOAT
{
    //! Narrows which agents one director governs.
    //!
    //! A director is global on its own. A component beside it attaches one of these, and several
    //! attached filters combine with AND, so narrowing is composed rather than authored on the
    //! director itself. A filter that cannot answer accepts, which keeps a broken setup loud
    //! without letting it silently change who is governed.
    class IDirectorFilter
    {
    public:
        AZ_RTTI(IDirectorFilter, IDirectorFilterTypeId);

        virtual ~IDirectorFilter() = default;

        //! True when this filter would let the director govern that agent.
        //!
        //! The entity comes with the agent because every filter needs it and the registry is
        //! already holding it. Called once per agent in the level on the director's own tick,
        //! so it must answer without blocking.
        virtual bool Accepts(AgentId agent, AZ::EntityId entity) const = 0;
    };
} // namespace GOAT
