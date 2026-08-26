#include <Core/Assets/BlackboardAssetHandler.h>

namespace GOAT
{
    BlackboardAssetHandler::BlackboardAssetHandler()
        : AzFramework::GenericAssetHandler<BlackboardAsset>(
              BlackboardAsset::DisplayName, BlackboardAsset::AssetGroup, BlackboardAsset::FileExtension)
    {
        // Lets the Asset Processor build the source into the cache without a custom builder.
        SetAutoBuildAssetToCache(true);
    }

    const char* BlackboardAssetHandler::GetBrowserIcon() const
    {
        return "Editor/Icons/GOAT/AssetBrowser/Blackboard.svg";
    }
} // namespace GOAT
