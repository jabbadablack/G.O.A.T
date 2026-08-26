#pragma once

#include <GOAT/Domain/BlackboardKey.h>

namespace GOAT
{
    class IBlackboardSystem;
}

namespace GOAT_Navigation
{
    //! The blackboard variables this module owns, resolved once and shared by every verb.
    //!
    //! Movement publishes through the blackboard rather than through action parameters so a
    //! project with its own character controller can read the same values GOAT steers by.
    struct NavigationKeys final
    {
        //! Declares the variables and caches their keys. Safe to call more than once.
        bool Declare(GOAT::IBlackboardSystem& blackboard);

        //! True once every key resolved.
        bool IsValid() const;

        //! Next point along the current path, in world space.
        GOAT::BlackboardKey m_waypoint;
        //! Distance left along the current path.
        GOAT::BlackboardKey m_remaining;
        //! False when the project moves the entity itself and GOAT should only publish.
        GOAT::BlackboardKey m_steer;
    };
} // namespace GOAT_Navigation
