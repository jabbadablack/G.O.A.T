#include <Core/Assets/BlackboardAssetHandler.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    BlackboardAssetHandler::BlackboardAssetHandler()
        : AzFramework::GenericAssetHandler<BlackboardAsset>(
              BlackboardAsset::DisplayName, BlackboardAsset::AssetGroup, BlackboardAsset::FileExtension)
    {
        // Lets the Asset Processor build the source into the cache without a custom builder.
        // The copy route would work too, but it marks dependencies handled, so an Asset<>
        // field inside a .bbx would silently produce no product dependency.
        SetAutoBuildAssetToCache(true);

        AZ_Assert(BlackboardAsset::FileExtension != nullptr && BlackboardAsset::FileExtension[0] != '\0',
            "A generic asset handler needs a file extension to claim");
    }

    const char* BlackboardAssetHandler::GetBrowserIcon() const
    {
        return "Editor/Icons/GOAT/AssetBrowser/Blackboard.svg";
    }
} // namespace GOAT
