#include <Navigation/ReachFilters.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT_Navigation
{
    namespace
    {
        //! How long a measured path distance is trusted before it is measured again.
        //! Long, because a director reconsiders at about a hertz and a stale metre never changes
        //! which agents it commands.
        AZ_CVAR(float, goat_reachPathSeconds, 3.0f, nullptr, AZ::ConsoleFunctorFlags::Null,
            "How long GOAT trusts a measured path distance when filtering a director's reach");
    } // namespace

    PathDistanceFilter::PathDistanceFilter(NavigationService& service)
        : m_service(service)
    {
    }

    AZ::Name PathDistanceFilter::GetName() const
    {
        return AZ_NAME_LITERAL("path_distance");
    }

    bool PathDistanceFilter::IsInReach(
        [[maybe_unused]] GOAT::AgentId director,
        const AZ::Vector3& directorPosition,
        GOAT::AgentId agent,
        const AZ::Vector3& agentPosition,
        float range) const
    {
        AZ_Assert(!agent.IsNull(), "A reach test is only asked about a registered agent");

        Measurement& measured = m_measurements[agent];

        // Collect an answer that arrived since last time. The path itself is thrown away: only
        // how long it was matters here.
        if (measured.m_request != InvalidPathRequestId)
        {
            AZStd::vector<AZ::Vector3> path;
            if (m_service.TakePath(measured.m_request, path))
            {
                measured.m_request = InvalidPathRequestId;
                measured.m_measuredAt = AZ::GetElapsedTimeMs();

                float length = 0.0f;
                for (size_t i = 1; i < path.size(); ++i)
                {
                    length += path[i].GetDistance(path[i - 1]);
                }

                // An empty path means nowhere to walk, which is out of reach however close the
                // agent looks -- exactly the case this filter exists for.
                measured.m_distance = path.empty() ? -1.0f : length;
            }
        }

        const AZ::TimeMs age = AZ::GetElapsedTimeMs() - measured.m_measuredAt;
        const auto trusted = AZ::TimeMs{ static_cast<AZ::s64>(goat_reachPathSeconds * 1000.0f) };

        if (measured.m_request == InvalidPathRequestId && age >= trusted)
        {
            measured.m_request = m_service.RequestPath(directorPosition, agentPosition);
        }

        if (measured.m_measuredAt == AZ::TimeMs{ 0 })
        {
            // Nothing measured yet, so answer the way the core would have. Refusing instead
            // would make a director govern nobody for its first tick.
            return directorPosition.GetDistance(agentPosition) <= range;
        }

        return measured.m_distance >= 0.0f && measured.m_distance <= range;
    }

    AZ::Name AheadOfFilter::GetName() const
    {
        return AZ_NAME_LITERAL("ahead_of");
    }

    bool AheadOfFilter::IsInReach(
        GOAT::AgentId director,
        const AZ::Vector3& directorPosition,
        [[maybe_unused]] GOAT::AgentId agent,
        const AZ::Vector3& agentPosition,
        [[maybe_unused]] float range) const
    {
        // The facing wanted is the director's, not the agent's, which is why the interface names
        // both parties rather than handing over two bare points.
        GOAT::IAgentSystem* agents = GOAT::AgentSystemInterface::Get();
        const AZ::EntityId entity = agents != nullptr ? agents->GetAgentEntity(director) : AZ::EntityId{};

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(transform, entity, &AZ::TransformInterface::GetWorldTM);

        const AZ::Vector3 toAgent = agentPosition - directorPosition;
        if (toAgent.IsZero())
        {
            return true;
        }

        return transform.GetBasisY().Dot(toAgent.GetNormalized()) > 0.0f;
    }
} // namespace GOAT_Navigation
