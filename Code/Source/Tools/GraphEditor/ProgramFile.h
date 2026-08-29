#pragma once

#include <GOAT/Assets/ProgramAsset.h>

#include <AzCore/std/string/string.h>

namespace GOAT::GraphEditor
{
    //! The root a program starts as before anything is authored into it.
    AuthoredNode DefaultRoot();

    //! Writes a program to a .goat file.
    bool SaveProgramFile(const AZStd::string& fullPath, const ProgramAsset& asset);

    //! Reads one back. False when the file is missing or is not a program.
    bool LoadProgramFile(const AZStd::string& fullPath, ProgramAsset& asset);

    //! A .goat path in a folder that no file already uses.
    AZStd::string UnusedProgramPath(const AZStd::string& folder, const AZStd::string& baseName);
} // namespace GOAT::GraphEditor
