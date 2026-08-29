#pragma once

#include <GOAT/Assets/ProgramAsset.h>

#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/string/string.h>

namespace GOAT::GraphEditor
{
    //! What is wrong with an authored program, and where.
    struct ValidationResult final
    {
        //! True when the program compiles.
        bool m_valid = true;
        //! Why it does not, in the compiler's own words.
        AZStd::string m_error;
        //! Path of child indices from the root to the node at fault, empty when the fault is
        //! the root's or the compiler did not say.
        AZStd::vector<size_t> m_path;
    };

    //! Compiles an authored program through whichever backend owns its root word.
    //! The real compiler rather than a second set of rules, so what the editor calls invalid
    //! is exactly what would fail to load.
    ValidationResult Validate(const AuthoredNode& root, const AZ::Name& programName);

    //! The backend that gives a program's root word meaning, or an empty name when nothing does.
    AZ::Name FindOwningBackend(const AuthoredNode& root);
} // namespace GOAT::GraphEditor
