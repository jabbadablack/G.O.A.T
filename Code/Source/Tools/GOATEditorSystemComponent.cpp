
#include <AzCore/Serialization/SerializeContext.h>
#include "GOATEditorSystemComponent.h"

#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/std/string/wildcard.h>

namespace GOAT
{
    AZ_COMPONENT_IMPL(GOATEditorSystemComponent, "GOATEditorSystemComponent",
        GOATEditorSystemComponentTypeId, BaseSystemComponent);

    void GOATEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GOATEditorSystemComponent, GOATSystemComponent>()
                ->Version(0);
        }
    }

    GOATEditorSystemComponent::GOATEditorSystemComponent() = default;

    GOATEditorSystemComponent::~GOATEditorSystemComponent() = default;

    void GOATEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("GOATEditorService"));
    }

    void GOATEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("GOATEditorService"));
    }

    void GOATEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void GOATEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

    void GOATEditorSystemComponent::Activate()
    {
        GOATSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
        AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus::Handler::BusConnect();
    }

    void GOATEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        GOATSystemComponent::Deactivate();
    }

    AzToolsFramework::AssetBrowser::SourceFileDetails GOATEditorSystemComponent::GetSourceFileDetails(
        const char* fullSourceFileName)
    {
        if (AZStd::wildcard_match("*.bbx", fullSourceFileName))
        {
            return AzToolsFramework::AssetBrowser::SourceFileDetails(
                "Editor/Icons/GOAT/AssetBrowser/Blackboard.svg");
        }
        return {};
    }
} // namespace GOAT
