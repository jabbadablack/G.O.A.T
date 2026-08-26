#include "GOATSystemComponent.h"

#include <GOAT/Assets/BehaviorTreeAsset.h>
#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/Domain/ActionPlan.h>
#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/Domain/BlackboardTypes.h>
#include <GOAT/Domain/Guard.h>
#include <GOAT/Domain/Intent.h>
#include <GOAT/Domain/NodeType.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Asset/GenericAssetHandler.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATSystemComponent, "GOATSystemComponent", GOATSystemComponentTypeId);

    void GOATSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        ReflectBlackboardTypes(context);
        ReflectActionTypes(context);
        ReflectGuardTypes(context);
        BlackboardKey::Reflect(context);
        Intent::Reflect(context);
        ActionPlan::Reflect(context);
        ReflectNodeTypes(context);
        BlackboardAsset::Reflect(context);
        BehaviorTreeAsset::Reflect(context);

        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOATSystemComponent, AZ::Component>()->Version(0);
        }
    }

    void GOATSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GOATService"));
        // Signals that this component registers generic asset handlers, so builders wait for it.
        provided.push_back(AzFramework::s_GenericAssetRegistrar);
    }

    void GOATSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GOATService"));
    }

    void GOATSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void GOATSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
    }

    GOATSystemComponent::GOATSystemComponent()
    {
        if (GOATInterface::Get() == nullptr)
        {
            GOATInterface::Register(this);
        }
    }

    GOATSystemComponent::~GOATSystemComponent()
    {
        if (GOATInterface::Get() == this)
        {
            GOATInterface::Unregister(this);
        }
    }

    void GOATSystemComponent::Init()
    {
    }

    void GOATSystemComponent::Activate()
    {
        m_blackboardSystem = AZStd::make_unique<BlackboardSystem>();
        RegisterAssetHandlers();
        GOATRequestBus::Handler::BusConnect();
    }

    void GOATSystemComponent::Deactivate()
    {
        GOATRequestBus::Handler::BusDisconnect();
        UnregisterAssetHandlers();
        m_blackboardSystem.reset();
    }

    void GOATSystemComponent::RegisterAssetHandlers()
    {
        // The launcher loads one module and the editor another, so only the first registration wins.
        if (AZ::Data::AssetManager::Instance().GetHandler(azrtti_typeid<BlackboardAsset>()) != nullptr)
        {
            return;
        }

        auto handler = AZStd::make_unique<AzFramework::GenericAssetHandler<BlackboardAsset>>(
            BlackboardAsset::DisplayName, BlackboardAsset::AssetGroup, BlackboardAsset::FileExtension);
        // Lets the Asset Processor build the source into the cache without a custom builder.
        handler->SetAutoBuildAssetToCache(true);
        handler->Register();
        m_assetHandlers.emplace_back(AZStd::move(handler));
    }

    void GOATSystemComponent::UnregisterAssetHandlers()
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
