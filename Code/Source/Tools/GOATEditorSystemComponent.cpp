
#include <AzCore/Serialization/SerializeContext.h>
#include "GOATEditorSystemComponent.h"

#include <GOAT/Assets/BlackboardAsset.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/string/wildcard.h>

#include <Tools/GraphEditor/GraphContext.h>
#include <Tools/GraphEditor/ProgramNode.h>
#include <Tools/GraphEditor/ProgramNodePaletteItem.h>
#include <Tools/GraphEditor/MainWindow.h>

#include <AzToolsFramework/API/ViewPaneOptions.h>

#include <LyViewPaneNames.h>

#include <QRect>

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

        GraphEditor::ProgramNode::Reflect(context);
        GraphEditor::CreateProgramNodeMimeEvent::Reflect(context);
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
        GraphEditor::GraphContext::SetInstance(AZStd::make_shared<GraphEditor::GraphContext>());
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
        AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus::Handler::BusConnect();
    }

    void GOATEditorSystemComponent::Deactivate()
    {
        GraphEditor::GraphContext::SetInstance(nullptr);
        AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus::Handler::BusDisconnect();
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        GOATSystemComponent::Deactivate();
    }

    void GOATEditorSystemComponent::NotifyRegisterViews()
    {
        AzToolsFramework::ViewPaneOptions options;
        options.paneRect = QRect(100, 100, 1280, 1024);
        options.showOnToolsToolbar = true;
        options.toolbarIcon = "Editor/Icons/GOAT/AssetBrowser/Program.svg";

        AzToolsFramework::RegisterViewPane<GraphEditor::MainWindow>(
            "GOAT Program Editor", LyViewPane::CategoryTools, options);
    }

    AzToolsFramework::AssetBrowser::SourceFileDetails GOATEditorSystemComponent::GetSourceFileDetails(
        const char* fullSourceFileName)
    {
        AZ_Assert(fullSourceFileName != nullptr, "The asset browser always asks about a named file");
        if (fullSourceFileName == nullptr)
        {
            return {};
        }

        // Source rows are not covered by the handler's product icon, so they are answered here.
        if (AZStd::wildcard_match("*.bbx", fullSourceFileName))
        {
            return AzToolsFramework::AssetBrowser::SourceFileDetails(
                "Editor/Icons/GOAT/AssetBrowser/Blackboard.svg");
        }
        if (AZStd::wildcard_match("*.goat", fullSourceFileName))
        {
            return AzToolsFramework::AssetBrowser::SourceFileDetails(
                "Editor/Icons/GOAT/AssetBrowser/Program.svg");
        }
        return {};
    }
} // namespace GOAT
