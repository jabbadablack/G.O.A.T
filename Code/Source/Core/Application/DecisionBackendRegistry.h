#pragma once

#include <Core/Application/NamedRegistry.h>

#include <GOAT/Interfaces/IDecisionBackend.h>

namespace GOAT
{
    //! The decision backends currently installed.
    using DecisionBackendRegistry = NamedRegistry<IDecisionBackend>;
} // namespace GOAT
