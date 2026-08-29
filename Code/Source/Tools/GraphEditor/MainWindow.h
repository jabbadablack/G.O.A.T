#pragma once

#include <GOAT/Assets/ProgramAsset.h>
#include <GOAT/GOATProgramEditorBus.h>
#include <Tools/GraphEditor/AgentBrowserPanel.h>
#include <Tools/GraphEditor/AgentDebugSource.h>
#include <Tools/GraphEditor/ProgramGraphSerializer.h>
#include <Tools/GraphEditor/RunningHighlight.h>
#include <Tools/GraphEditor/Core.h>

#include <GraphCanvas/Widgets/GraphCanvasEditor/GraphCanvasAssetEditorMainWindow.h>
#include <GraphModel/Integration/EditorMainWindow.h>

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>

class QTimer;

namespace GOAT::GraphEditor
{
    //! Builds the palette out of whatever the node type registry currently holds, so a backend
    //! that registers a word gets a palette entry without this file knowing about it.
    struct ProgramEditorConfig final
        : GraphCanvas::AssetEditorWindowConfig
    {
        GraphCanvas::GraphCanvasTreeItem* CreateNodePaletteRoot() override;
    };

    //! The GOAT Program Editor window.
    class MainWindow
        : public GraphModelIntegration::EditorMainWindow
        , public GOATProgramEditorRequestBus::Handler
    {
        Q_OBJECT // AUTOMOC
    public:
        AZ_CLASS_ALLOCATOR(MainWindow, AZ::SystemAllocator);

        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow() override;

        // GOATProgramEditorRequestBus
        void OpenProgram(const AZStd::string& fullPath) override;

    protected:
        // GraphModelIntegration::EditorMainWindow
        GraphModel::GraphContextPtr GetGraphContext() const override;
        void OnEditorOpened(GraphCanvas::EditorDockWidget* dockWidget) override;
        void OnEditorClosing(GraphCanvas::EditorDockWidget* dockWidget) override;

        // GraphCanvas::AssetEditorMainWindow menu hooks
        QAction* AddFileNewAction(QMenu* menu) override;
        QAction* AddFileOpenAction(QMenu* menu) override;
        QAction* AddFileSaveAction(QMenu* menu) override;

        // GraphControllerNotificationBus
        void OnGraphModelNodeAdded(GraphModel::NodePtr node) override;
        void OnGraphModelGraphModified(GraphModel::NodePtr node) override;
        void OnGraphModelRequestUndoPoint() override;
        void OnGraphModelTriggerUndo() override;
        void OnGraphModelTriggerRedo() override;

    private:
        //! One saved state of the whole graph. GraphCanvas batches the undo points; what a
        //! point holds is the client's business, and a whole graph is the only shape that
        //! survives node, slot and connection edits alike.
        using Snapshot = AZStd::vector<AZ::u8>;

        //! What the active canvas currently holds, read back into an authored program.
        AZ::Outcome<AuthoredNode, AZStd::string> ReadActiveGraph() const;

        //! Asks for a validation pass once the current edit has settled.
        //! Queued rather than immediate so a burst of changes is answered once.
        void QueueRevalidate();

        //! Runs the program through its backend and paints whatever failed.
        void Revalidate();

        //! Paints one node in a named graph.
        void Paint(const GraphCanvas::GraphId& graphId, GraphModel::NodePtr node, const char* palette);

        //! The node a path of child indices leads to, or null when it leads nowhere.
        GraphModel::NodePtr NodeAtPath(const AZStd::vector<size_t>& path) const;

        Snapshot Capture() const;
        void Restore(const Snapshot& snapshot);
        //! Restores once the call that asked for it has returned, because restoring destroys the
        //! graph controller that asked.
        void QueueRestore(const Snapshot& snapshot);

        //! Loads and saves the .goat file the active canvas is a view of.
        void OnNewProgram();
        void OnOpenProgram();
        void OnSaveProgram();
        //! Opens a program Lua declared, which the canvas shows but never writes back.
        void OnOpenLuaProgram();

        void LoadAuthored(const AuthoredNode& root, const AZStd::string& name, bool readOnly);

        //! Reads the debug source and shows what it says. Driven by m_pollTimer.
        void PollDebugSource();

        //! Lights the path the selected agent is running, or puts the light out when there is
        //! no agent, no program on screen, or the agent is running a different one.
        void RefreshHighlight();

        //! Shows the program an agent is running, read only, from what the runtime compiled.
        void OpenRunningProgram(const AZStd::string& name);

        //! Opens the program a newly picked agent is running, when it is not already open.
        void OnSelectedAgentChanged();

        //! Builds the menu that chooses what is being watched.
        void BuildDebugMenu();

        //! Swaps what agent state is read from, dropping whatever the old one was showing.
        void WatchThisEditor();
        void AttachToLauncher();

        //! Spaces nodes out using the size each one actually drew at.
        //! The flattening pass can only guess, and a node with several properties on its face is
        //! far taller than any guess, so siblings laid out by a fixed row height overlap.
        void LayoutByMeasuredSize(const GraphCanvas::GraphId& graphId,
            const AZStd::vector<PlacedNode>& placed,
            const AZStd::vector<GraphModel::NodePtr>& added) const;

        AZStd::string m_programName;
        AZStd::string m_programPath;
        bool m_readOnly = false;
        //! Nodes painted by the last validation pass, so the paint can be cleared again.
        AZStd::vector<GraphModel::NodePtr> m_painted;

        //! Where agent state is read from, and what it drives.
        AZStd::unique_ptr<IAgentDebugSource> m_debug;
        AgentBrowserPanel* m_agents = nullptr;
        //! The agent whose program is on screen, so that a poll re-selecting a row is never
        //! mistaken for the user picking a different agent.
        AgentId m_watching;
        RunningHighlight m_highlight;
        QTimer* m_pollTimer = nullptr;

        //! Counts loads, so a layout deferred by one can tell that another has replaced the
        //! graph it was measuring and step aside rather than lay out a canvas that is gone.
        AZ::u32 m_loadGeneration = 0;

        AZStd::vector<Snapshot> m_undo;
        AZStd::vector<Snapshot> m_redo;
        //! True while an undo or a load is rebuilding the graph, so it does not record itself.
        bool m_restoring = false;
        //! True while validating. Painting a node reports the graph as modified, which asks for
        //! validation again, so without this a single edit recurses until the stack runs out.
        bool m_validating = false;
        //! True when a validation pass is already waiting to run.
        bool m_revalidateQueued = false;
    };
} // namespace GOAT::GraphEditor
