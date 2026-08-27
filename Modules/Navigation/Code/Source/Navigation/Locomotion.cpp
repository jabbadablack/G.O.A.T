#include <Navigation/Locomotion.h>

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Debug/Trace.h>

namespace GOAT_Navigation
{
    void Locomotion::Steer(AZ::EntityId entity, const AZ::Vector3& waypoint, float speed)
    {
        AZ_Assert(entity.IsValid(), "Only a real entity can be carried");
        AZ_Assert(speed > 0.0f, "Carrying an agent needs a positive speed");
        if (!entity.IsValid() || speed <= 0.0f)
        {
            return;
        }

        Target& target = m_moving[entity];
        target.m_waypoint = waypoint;
        target.m_speed = speed;
    }

    void Locomotion::Stop(AZ::EntityId entity)
    {
        m_moving.erase(entity);
    }

    void Locomotion::Advance(float deltaTime)
    {
        AZ_Assert(deltaTime >= 0.0f, "A frame cannot run backwards");
        if (deltaTime <= 0.0f)
        {
            return;
        }

        for (auto& [entity, target] : m_moving)
        {
            AZ::Vector3 position = AZ::Vector3::CreateZero();
            AZ::TransformBus::EventResult(position, entity, &AZ::TransformInterface::GetWorldTranslation);

            const AZ::Vector3 toWaypoint = target.m_waypoint - position;
            const float distance = toWaypoint.GetLength();
            if (distance <= 0.0f)
            {
                continue;
            }

            // Never overshoot: the agent stops on the waypoint and waits there until its next
            // tick names another one, which is what keeps it from oscillating across a corner.
            const float travel = target.m_speed * deltaTime;
            const AZ::Vector3 next =
                travel >= distance ? target.m_waypoint : position + toWaypoint.GetNormalized() * travel;

            AZ::TransformBus::Event(entity, &AZ::TransformInterface::SetWorldTranslation, next);
        }
    }
} // namespace GOAT_Navigation
