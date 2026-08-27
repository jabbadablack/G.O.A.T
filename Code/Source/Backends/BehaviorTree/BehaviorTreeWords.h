#pragma once

#include <GOAT/VocabularyScope.h>

#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! The words a behaviour tree is written in. Anything that is not a tree idea -- a condition,
    //! a verb, a delegate -- stays in the core, because every paradigm reads it.
    AZStd::vector<NodeTypeDescriptor> BehaviorTreeWords();

    //! Installs them, so they leave again with the backend that reads them.
    bool InstallBehaviorTreeWords(VocabularyScope& vocabulary);
} // namespace GOAT
