#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/std/containers/unordered_map.h>

namespace GOAT_Navigation
{
    //! Carries agents toward the waypoint their path put them on, once per frame.
    //!
    //! Deciding where to go happens on an agent's own pacing band -- ten times a second for a
    //! near one, once a second for a distant one -- and moving it on that same beat makes it
    //! cover the whole interval in one jump. At a hundred agents that reads as the crowd
    //! snapping between positions rather than walking.
    //!
    //! Moving is not a decision. It is what carries out the last one, so it belongs on the frame
    //! while the decision stays on the band. That split is the point of the bands: an agent can
    //! think four times a second and still move smoothly.
    class Locomotion final
    {
    public:
        AZ_CLASS_ALLOCATOR(Locomotion, AZ::SystemAllocator);

        //! Points an agent at a waypoint. Called whenever the agent ticks, which is as often as
        //! its band allows; the frames in between are what this class exists to fill.
        void Steer(AZ::EntityId entity, const AZ::Vector3& waypoint, float speed);

        //! Stops carrying an agent, because its move ended or was interrupted.
        void Stop(AZ::EntityId entity);

        //! Moves everything being carried. One frame's worth.
        void Advance(float deltaTime);

        //! How many agents are moving, for the console and for tests.
        size_t GetMovingCount() const { return m_moving.size(); }

    private:
        //! Where one agent is headed and how fast.
        struct Target final
        {
            AZ::Vector3 m_waypoint = AZ::Vector3::CreateZero();
            float m_speed = 0.0f;
        };

        AZStd::unordered_map<AZ::EntityId, Target> m_moving;
    };
} // namespace GOAT_Navigation
