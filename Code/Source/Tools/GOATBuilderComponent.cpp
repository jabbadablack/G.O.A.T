#include <Tools/GOATBuilderComponent.h>

#include <Core/Assets/BlackboardAssetHandler.h>
#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/GOATTypeIds.h>

#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Asset/GenericAssetHandler.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATBuilderComponent, "GOATBuilderComponent", GOATBuilderComponentTypeId);

    void GOATBuilderComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            // The tag is what makes the Asset Processor activate this in a builder process.
            serializeContext->Class<GOATBuilderComponent, AZ::Component>()
                ->Version(0)
                ->Attribute(
                    AZ::Edit::Attributes::SystemComponentTags,
                    AZStd::vector<AZ::Crc32>({ AssetBuilderSDK::ComponentTags::AssetBuilder }));
        }
    }

    void GOATBuilderComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        // The generic asset builder waits on this before it enumerates registered handlers.
        provided.push_back(AzFramework::s_GenericAssetRegistrar);
    }

    GOATBuilderComponent::GOATBuilderComponent() = default;

    GOATBuilderComponent::~GOATBuilderComponent() = default;

    void GOATBuilderComponent::Activate()
    {
        if (!AZ::Data::AssetManager::IsReady() ||
            AZ::Data::AssetManager::Instance().GetHandler(azrtti_typeid<BlackboardAsset>()) != nullptr)
        {
            return;
        }

        auto handler = AZStd::make_unique<BlackboardAssetHandler>();
        handler->Register();
        m_assetHandlers.emplace_back(AZStd::move(handler));
    }

    void GOATBuilderComponent::Deactivate()
    {
        if (AZ::Data::AssetManager::IsReady())
        {
            for (auto& handler : m_assetHandlers)
            {
                AZ::Data::AssetManager::Instance().UnregisterHandler(handler.get());
            }
        }
        m_assetHandlers.clear();
    }
} // namespace GOAT
