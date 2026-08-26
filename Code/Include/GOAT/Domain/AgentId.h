#pragma once

#include <GOAT/Domain/Handle.h>

namespace GOAT
{
    //! Distinguishes agent handles from every other kind of handle.
    struct AgentTag
    {
    };

    //! Identifies one registered agent for the lifetime of that agent.
    using AgentId = Handle<AgentTag>;
} // namespace GOAT
