#pragma once

#include <Core/Application/NamedRegistry.h>

#include <GOAT/Interfaces/IBackend.h>

namespace GOAT
{
    //! The decision backends currently installed.
    //! Adding one is a registration; removing one is deleting its folder.
    using BackendRegistry = NamedRegistry<IBackend>;
} // namespace GOAT
