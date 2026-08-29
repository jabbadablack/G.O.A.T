#pragma once

#include <GOAT/VocabularyScope.h>

#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! The words a task network is written in. Conditions and verbs stay in the core, because
    //! every paradigm reads them.
    AZStd::vector<NodeTypeDescriptor> HtnWords();

    //! Installs them, so they leave again with the backend that reads them.
    bool InstallHtnWords(VocabularyScope& vocabulary);
} // namespace GOAT
