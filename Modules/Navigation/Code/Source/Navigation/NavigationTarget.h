#pragma once

#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Math/Vector3.h>

namespace GOAT_Navigation
{
    //! Where an action was pointed: its blackboard key when it has one, its literal otherwise.
    //! Shared by every spatial verb so they all read a target the same way.
    //! @return false when a named blackboard variable could not be read for this agent.
    bool ReadActionTarget(const GOAT::ActionContext& context, AZ::Vector3& outTarget);

    //! The agent entity's current world position.
    AZ::Vector3 ReadActionPosition(const GOAT::ActionContext& context);
} // namespace GOAT_Navigation
