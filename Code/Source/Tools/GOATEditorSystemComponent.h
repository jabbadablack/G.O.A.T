
#pragma once

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/AssetBrowser/AssetBrowserBus.h>

#include <Clients/GOATSystemComponent.h>

namespace GOAT
{
    /// System component for GOAT editor
    class GOATEditorSystemComponent
        : public GOATSystemComponent
        , protected AzToolsFramework::EditorEvents::Bus::Handler
        , protected AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus::Handler
    {
        using BaseSystemComponent = GOATSystemComponent;
    public:
        AZ_COMPONENT_DECL(GOATEditorSystemComponent);

        static void Reflect(AZ::ReflectContext* context);

        GOATEditorSystemComponent();
        ~GOATEditorSystemComponent();

    private:
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        //! Starts listening for a launcher, at startup rather than when one is first asked for:
        //! a launcher dials in on its own timer, so the editor has to already be listening or
        //! the first attempt to attach would always find nothing.
        void StartRemoteDebugHost();

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        // AzToolsFramework::EditorEvents
        //! Registers the tool windows under Tools.
        void NotifyRegisterViews() override;

        // AzToolsFramework::AssetBrowser::AssetBrowserInteractionNotificationBus
        //! Gives .bbx and .goat source files their own thumbnail. The handler's browser icon
        //! only covers the processed product, so without this the source row stays generic.
        AzToolsFramework::AssetBrowser::SourceFileDetails GetSourceFileDetails(const char* fullSourceFileName) override;

        //! Puts "GOAT Program" in the browser's Create menu, which is where a program is made.
        void AddSourceFileCreators(const char* fullSourceFolderName, const AZ::Uuid& sourceUUID,
            AzToolsFramework::AssetBrowser::SourceFileCreatorList& creators) override;

        //! Opens a .goat in the program editor rather than handing it to the operating system.
        void AddSourceFileOpeners(const char* fullSourceFileName, const AZ::Uuid& sourceUUID,
            AzToolsFramework::AssetBrowser::SourceFileOpenerList& openers) override;
    };
} // namespace GOAT
