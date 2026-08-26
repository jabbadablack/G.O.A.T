#pragma once

#include <GOAT/Assets/BlackboardAsset.h>

#include <AzCore/Memory/SystemAllocator.h>
#include <AzFramework/Asset/GenericAssetHandler.h>

namespace GOAT
{
    //! Handles .bbx assets, adding the icon the asset browser shows for them.
    //! Everything else is the generic behaviour, including being built into the cache.
    class BlackboardAssetHandler final
        : public AzFramework::GenericAssetHandler<BlackboardAsset>
    {
    public:
        AZ_CLASS_ALLOCATOR(BlackboardAssetHandler, AZ::SystemAllocator);

        BlackboardAssetHandler();

        //! Path of the icon shown for this asset type, relative to the asset cache.
        const char* GetBrowserIcon() const override;
    };
} // namespace GOAT
