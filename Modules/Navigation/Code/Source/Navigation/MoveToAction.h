#pragma once

#include <Navigation/Locomotion.h>
#include <Navigation/NavigationKeys.h>
#include <Navigation/NavigationService.h>
#include <Navigation/PathPool.h>

#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Memory/SystemAllocator.h>

namespace GOAT_Navigation
{
    //! Walks an agent to a position, publishing its progress to the blackboard.
    //!
    //! The target is the action's blackboard key when it has one, otherwise its literal
    //! position. `nav_waypoint` and `nav_remaining` are written every step whether or not this
    //! verb also moves the entity, so a project with its own controller sets `nav_steer` false
    //! and drives movement from those two values instead.
    //!
    //! This verb chooses the waypoint; Locomotion carries the agent there frame by frame, so an
    //! agent thinking once a second still walks smoothly.
    class MoveToAction final
        : public GOAT::IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(MoveToAction, AZ::SystemAllocator);

        MoveToAction(NavigationService& service, PathPool& paths, const NavigationKeys& keys, Locomotion& locomotion);

        AZ::Name GetName() const override;
        void Begin(const GOAT::ActionContext& context) override;
        GOAT::ActionResult Step(const GOAT::ActionContext& context, float deltaTime) override;
        void End(const GOAT::ActionContext& context) override;

    private:
        //! Moves the collected path into a pooled slot once the query answers.
        //! Returns Running while the query is still out.
        GOAT::ActionResult CollectPath(const GOAT::ActionContext& context);

        //! Advances along an already collected path.
        GOAT::ActionResult FollowPath(const GOAT::ActionContext& context, float deltaTime);

        //! Publishes the next waypoint and the distance left for this agent.
        void Publish(const GOAT::ActionContext& context, const AZ::Vector3& waypoint, float remaining) const;

        NavigationService& m_service;
        PathPool& m_paths;
        const NavigationKeys& m_keys;
        //! Where the moving happens. This verb decides the waypoint on the agent's band; the
        //! frames in between are carried there.
        Locomotion& m_locomotion;
    };
} // namespace GOAT_Navigation
