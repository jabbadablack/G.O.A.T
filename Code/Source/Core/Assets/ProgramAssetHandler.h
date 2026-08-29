#pragma once

#include <GOAT/Assets/ProgramAsset.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzFramework/Asset/GenericAssetHandler.h>

namespace GOAT
{
    //! Handles .goat assets, adding the icon the asset browser shows for them.
    //! Everything else is the generic behaviour, including being built into the cache.
    class ProgramAssetHandler final
        : public AzFramework::GenericAssetHandler<ProgramAsset>
    {
    public:
        AZ_CLASS_ALLOCATOR(ProgramAssetHandler, AZ::SystemAllocator);

        ProgramAssetHandler();

        //! Path of the icon shown for this asset type, relative to the asset cache.
        const char* GetBrowserIcon() const override;
    };
} // namespace GOAT
