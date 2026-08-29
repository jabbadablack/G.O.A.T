#include <Core/Assets/ProgramAssetHandler.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    ProgramAssetHandler::ProgramAssetHandler()
        : AzFramework::GenericAssetHandler<ProgramAsset>(
              ProgramAsset::DisplayName, ProgramAsset::AssetGroup, ProgramAsset::FileExtension)
    {
        SetAutoBuildAssetToCache(true);

        AZ_Assert(ProgramAsset::FileExtension != nullptr && ProgramAsset::FileExtension[0] != '\0',
            "A generic asset handler needs a file extension to claim");
    }

    const char* ProgramAssetHandler::GetBrowserIcon() const
    {
        return "Editor/Icons/GOAT/AssetBrowser/Program.svg";
    }
} // namespace GOAT
