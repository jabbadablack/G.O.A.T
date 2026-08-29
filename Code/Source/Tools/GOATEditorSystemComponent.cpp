
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
#include <Tools/GraphEditor/ProgramFile.h>

#include <GOAT/GOATProgramEditorBus.h>

#include <AzCore/IO/Path/Path.h>
#include <AzCore/StringFunc/StringFunc.h>
#include <AzToolsFramework/API/ViewPaneOptions.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserBus.h>

#include <LyViewPaneNames.h>

#include <QIcon>
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

    void GOATEditorSystemComponent::AddSourceFileCreators(
        [[maybe_unused]] const char* fullSourceFolderName, [[maybe_unused]] const AZ::Uuid& sourceUUID,
        AzToolsFramework::AssetBrowser::SourceFileCreatorList& creators)
    {
        auto create = [](const AZStd::string& folder, [[maybe_unused]] const AZ::Uuid& uuid)
        {
            const AZStd::string path = GraphEditor::UnusedProgramPath(folder, "NewProgram");

            AZStd::string name;
            AZ::StringFunc::Path::GetFileName(path.c_str(), name);

            ProgramAsset asset;
            asset.m_name = name;
            asset.m_root = GraphEditor::DefaultRoot();

            if (!GraphEditor::SaveProgramFile(path, asset))
            {
                AZ_Error("GOAT", false, "The new program could not be written to %s", path.c_str());
                return;
            }

            // Lets the browser select the new row and start its inline rename.
            AzToolsFramework::AssetBrowser::AssetBrowserFileCreationNotificationBus::Event(
                AzToolsFramework::AssetBrowser::AssetBrowserFileCreationNotifications::FileCreationNotificationBusId,
                &AzToolsFramework::AssetBrowser::AssetBrowserFileCreationNotifications::HandleAssetCreatedInEditor,
                path, AZ::Crc32(), false);

            AzToolsFramework::EditorRequests::Bus::Broadcast(
                &AzToolsFramework::EditorRequests::OpenViewPane, "GOAT Program Editor");
            GOATProgramEditorRequestBus::Broadcast(&GOATProgramEditorRequests::OpenProgram, path);
        };

        creators.push_back({ "GOAT_Program_creator", "GOAT Program",
            QIcon("Editor/Icons/GOAT/AssetBrowser/Program.svg"), create });
    }

    void GOATEditorSystemComponent::AddSourceFileOpeners(
        const char* fullSourceFileName, [[maybe_unused]] const AZ::Uuid& sourceUUID,
        AzToolsFramework::AssetBrowser::SourceFileOpenerList& openers)
    {
        AZ_Assert(fullSourceFileName != nullptr, "The asset browser always asks about a named file");
        if (fullSourceFileName == nullptr ||
            !AZStd::wildcard_match("*.goat", fullSourceFileName))
        {
            return;
        }

        auto open = [](const char* path, [[maybe_unused]] const AZ::Uuid& uuid)
        {
            AzToolsFramework::EditorRequests::Bus::Broadcast(
                &AzToolsFramework::EditorRequests::OpenViewPane, "GOAT Program Editor");
            GOATProgramEditorRequestBus::Broadcast(
                &GOATProgramEditorRequests::OpenProgram, AZStd::string(path));
        };

        openers.push_back({ "GOAT_Program_opener", "GOAT Program Editor",
            QIcon("Editor/Icons/GOAT/AssetBrowser/Program.svg"), open });
    }
} // namespace GOAT
