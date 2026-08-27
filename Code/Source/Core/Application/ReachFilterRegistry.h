#pragma once

#include <Core/Application/NamedRegistry.h>

#include <GOAT/Interfaces/IReachFilter.h>

namespace GOAT
{
    //! Ways of narrowing a director's reach that the core cannot judge for itself.
    using ReachFilterRegistry = NamedRegistry<IReachFilter>;
} // namespace GOAT
