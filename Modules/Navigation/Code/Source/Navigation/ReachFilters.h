#pragma once

#include <Navigation/NavigationService.h>

#include <GOAT/Interfaces/IReachFilter.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Time/ITime.h>
#include <AzCore/std/containers/unordered_map.h>

namespace GOAT_Navigation
{
    //! Reach by how far an agent actually is to walk, rather than how far it looks.
    //!
    //! Straight line distance and path distance stop agreeing the moment a wall is involved, and
    //! a director that governs "everyone within thirty metres" almost always means the ones who
    //! could get here.
    //!
    //! Path queries are asynchronous and a reach test may not block, so this answers from a short
    //! lived per agent cache and starts a refresh when that answer is stale. The first answer for
    //! an agent is therefore the straight line one. That is honest rather than a shortcut: a
    //! director's reach is a decision about roughly whom to command, and a metre of slack in it
    //! never changes an order.
    class PathDistanceFilter final
        : public GOAT::IReachFilter
    {
    public:
        AZ_CLASS_ALLOCATOR(PathDistanceFilter, AZ::SystemAllocator);

        explicit PathDistanceFilter(NavigationService& service);

        AZ::Name GetName() const override;
        bool IsInReach(
            GOAT::AgentId director,
            const AZ::Vector3& directorPosition,
            GOAT::AgentId agent,
            const AZ::Vector3& agentPosition,
            float range) const override;

    private:
        //! What was last measured for one agent, and when.
        struct Measurement final
        {
            PathRequestId m_request = InvalidPathRequestId;
            float m_distance = -1.0f;
            AZ::TimeMs m_measuredAt{ 0 };
        };

        NavigationService& m_service;
        //! Mutable because a reach test is logically a question, and the caching is how it
        //! answers one without blocking.
        mutable AZStd::unordered_map<GOAT::AgentId, Measurement> m_measurements;
    };

    //! Reach only what the director is facing.
    //!
    //! Cheap and synchronous: a dot product against the director's forward axis. Useful for a
    //! director that represents a direction of advance rather than a place.
    class AheadOfFilter final
        : public GOAT::IReachFilter
    {
    public:
        AZ_CLASS_ALLOCATOR(AheadOfFilter, AZ::SystemAllocator);

        AZ::Name GetName() const override;
        bool IsInReach(
            GOAT::AgentId director,
            const AZ::Vector3& directorPosition,
            GOAT::AgentId agent,
            const AZ::Vector3& agentPosition,
            float range) const override;
    };
} // namespace GOAT_Navigation
