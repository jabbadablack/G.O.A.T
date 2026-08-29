#pragma once

#include <GOAT/VocabularyScope.h>

#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! The words a utility program is written in. The steps a choice runs are core words.
    AZStd::vector<NodeTypeDescriptor> UtilityWords();

    //! Installs them, so they leave again with the backend that reads them.
    bool InstallUtilityWords(VocabularyScope& vocabulary);
} // namespace GOAT
