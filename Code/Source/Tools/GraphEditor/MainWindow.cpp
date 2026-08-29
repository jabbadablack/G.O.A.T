#include <Tools/GraphEditor/MainWindow.h>
#include <Tools/GraphEditor/GraphContext.h>
#include <Tools/GraphEditor/ProgramGraphSerializer.h>
#include <Tools/GraphEditor/ProgramNode.h>
#include <Tools/GraphEditor/ProgramNodePaletteItem.h>
#include <Tools/GraphEditor/ProgramValidator.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <GraphCanvas/Components/Nodes/NodeTitleBus.h>
#include <GraphCanvas/Widgets/NodePalette/TreeItems/IconDecoratedNodePaletteTreeItem.h>
#include <GraphCanvas/Widgets/NodePalette/TreeItems/NodePaletteTreeItem.h>
#include <GraphModel/GraphModelBus.h>
#include <GraphModel/Model/Connection.h>
#include <GraphModel/Model/Graph.h>
#include <GraphModel/Model/Slot.h>

#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/std/containers/map.h>
#include <AzCore/std/sort.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>

#include <QAction>
#include <QInputDialog>
#include <QStatusBar>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>

namespace GOAT::GraphEditor
{
    namespace
    {
        //! How many undo points are kept. A snapshot is the whole graph, so the depth is
        //! bounded rather than the memory being left to grow with the session.
        constexpr size_t UndoDepth = 64;

        ProgramEditorConfig* MakeConfig()
        {
            auto* config = new ProgramEditorConfig();
            config->m_editorId = ProgramEditorId;
            config->m_baseStyleSheet = StyleSheet;
            config->m_mimeType = MimeEventType;
            config->m_saveIdentifier = SaveIdentifier;
            return config;
        }
    } // namespace

    GraphCanvas::GraphCanvasTreeItem* ProgramEditorConfig::CreateNodePaletteRoot()
    {
        auto* root = aznew GraphCanvas::NodePaletteTreeItem("Root", ProgramEditorId);

        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            return root;
        }

        // Grouped by the category each word declares, so a gem decides where its own words sit.
        AZStd::map<AZStd::string, AZStd::vector<const NodeTypeDescriptor*>> byCategory;
        for (const AZ::Name& name : agents->GetNodeTypeNames())
        {
            if (const NodeTypeDescriptor* descriptor = agents->FindNodeType(name); descriptor != nullptr)
            {
                const AZStd::string category =
                    descriptor->m_category.empty() ? AZStd::string("Other") : descriptor->m_category;
                byCategory[category].push_back(descriptor);
            }
        }

        for (auto& [category, words] : byCategory)
        {
            AZStd::sort(words.begin(), words.end(),
                [](const NodeTypeDescriptor* lhs, const NodeTypeDescriptor* rhs)
                { return lhs->m_name.GetStringView() < rhs->m_name.GetStringView(); });

            auto* group = root->CreateChildNode<GraphCanvas::IconDecoratedNodePaletteTreeItem>(
                category.c_str(), ProgramEditorId);
            group->SetTitlePalette(ProgramNode::TitlePalette(*words.front()));

            for (const NodeTypeDescriptor* descriptor : words)
            {
                auto* item = group->CreateChildNode<ProgramNodePaletteItem>(
                    AZStd::string(descriptor->m_name.GetCStr()), ProgramNode::TitlePalette(*descriptor),
                    ProgramEditorId);
                item->SetToolTip(QString::fromUtf8(descriptor->m_description.c_str()));
            }
        }

        return root;
    }

    MainWindow::MainWindow(QWidget* parent)
        : GraphModelIntegration::EditorMainWindow(MakeConfig(), parent)
    {
        SetupUI();
        SetDropAreaText("Create a new GOAT program, or open one from the File menu.");
    }

    MainWindow::~MainWindow() = default;

    GraphModel::GraphContextPtr MainWindow::GetGraphContext() const
    {
        return GraphContext::GetInstance();
    }

    void MainWindow::OnEditorOpened(GraphCanvas::EditorDockWidget* dockWidget)
    {
        EditorMainWindow::OnEditorOpened(dockWidget);
        m_undo.clear();
        m_redo.clear();
    }

    void MainWindow::OnEditorClosing(GraphCanvas::EditorDockWidget* dockWidget)
    {
        m_painted.clear();
        m_undo.clear();
        m_redo.clear();
        EditorMainWindow::OnEditorClosing(dockWidget);
    }

    void MainWindow::OnGraphModelNodeAdded(GraphModel::NodePtr node)
    {
        // Every word is drawn in its own paradigm's and kind's colour, which is the only cue
        // saying what a node is once the palette is closed.
        if (auto* program = azrtti_cast<ProgramNode*>(node.get()); program != nullptr)
        {
            if (const NodeTypeDescriptor* descriptor = program->GetDescriptor(); descriptor != nullptr)
            {
                Paint(node, ProgramNode::TitlePalette(*descriptor));
            }
        }
        Revalidate();
    }

    void MainWindow::OnGraphModelGraphModified([[maybe_unused]] GraphModel::NodePtr node)
    {
        Revalidate();
    }

    void MainWindow::OnGraphModelRequestUndoPoint()
    {
        if (m_restoring)
        {
            return;
        }

        m_undo.push_back(Capture());
        if (m_undo.size() > UndoDepth)
        {
            m_undo.erase(m_undo.begin());
        }
        m_redo.clear();
    }

    void MainWindow::OnGraphModelTriggerUndo()
    {
        if (m_undo.size() < 2)
        {
            return;
        }

        // The last point is what the graph already looks like, so undo steps past it.
        m_redo.push_back(m_undo.back());
        m_undo.pop_back();
        Restore(m_undo.back());
    }

    void MainWindow::OnGraphModelTriggerRedo()
    {
        if (m_redo.empty())
        {
            return;
        }

        Snapshot next = m_redo.back();
        m_redo.pop_back();
        m_undo.push_back(next);
        Restore(next);
    }

    MainWindow::Snapshot MainWindow::Capture() const
    {
        const GraphCanvas::GraphId graphId = GetActiveGraphCanvasGraphId();
        GraphModel::GraphPtr graph = GetGraphById(graphId);
        if (graph == nullptr)
        {
            return {};
        }

        GraphCanvas::GraphModelRequestBus::Event(
            graphId, &GraphCanvas::GraphModelRequests::OnSaveDataDirtied, graphId);

        Snapshot snapshot;
        AZ::IO::ByteContainerStream<Snapshot> stream(&snapshot);
        AZ::Utils::SaveObjectToStream(stream, AZ::ObjectStream::ST_BINARY, graph.get());
        return snapshot;
    }

    void MainWindow::Restore(const Snapshot& snapshot)
    {
        if (snapshot.empty())
        {
            return;
        }

        const GraphCanvas::GraphId graphId = GetActiveGraphCanvasGraphId();
        GraphModel::GraphContextPtr context = GetGraphContext();

        GraphModel::GraphPtr graph = AZStd::make_shared<GraphModel::Graph>(context);
        Snapshot copy = snapshot;
        AZ::Utils::LoadObjectFromBufferInPlace(copy.data(), copy.size(), *graph.get());

        m_restoring = true;
        m_painted.clear();

        // Each slot caches a pointer back to its node, which holds the outgoing graph alive
        // once anything has been connected on it.
        if (GraphModel::GraphPtr outgoing = GetGraphById(graphId); outgoing != nullptr)
        {
            outgoing->ClearCachedData();
        }

        graph->PostLoadSetup(context);
        GraphModelIntegration::GraphManagerRequestBus::Broadcast(
            &GraphModelIntegration::GraphManagerRequests::DeleteGraphController, graphId);
        GraphModelIntegration::GraphManagerRequestBus::Broadcast(
            &GraphModelIntegration::GraphManagerRequests::CreateGraphController, graphId, graph);
        m_restoring = false;

        Revalidate();
    }

    void MainWindow::Paint(GraphModel::NodePtr node, const char* palette) const
    {
        GraphCanvas::NodeId nodeId;
        GraphModelIntegration::GraphControllerRequestBus::EventResult(
            nodeId, GetActiveGraphCanvasGraphId(),
            &GraphModelIntegration::GraphControllerRequests::GetNodeIdByNode, node);
        if (!nodeId.IsValid())
        {
            return;
        }

        GraphCanvas::NodeTitleRequestBus::Event(
            nodeId, &GraphCanvas::NodeTitleRequests::SetPaletteOverride, AZStd::string(palette));
    }

    GraphModel::NodePtr MainWindow::NodeAtPath(const AZStd::vector<size_t>& path) const
    {
        const GraphCanvas::GraphId graphId = GetActiveGraphCanvasGraphId();
        GraphModel::GraphPtr graph = GetGraphById(graphId);
        if (graph == nullptr)
        {
            return nullptr;
        }

        auto positionOf = [graphId](GraphModel::ConstNodePtr node)
        {
            AZ::Vector2 position = AZ::Vector2::CreateZero();
            GraphModelIntegration::GraphControllerRequestBus::EventResult(
                position, graphId, &GraphModelIntegration::GraphControllerRequests::GetPosition,
                AZStd::const_pointer_cast<GraphModel::Node>(node));
            return position;
        };

        // Walk the same order the reader wrote the path in: services, then children.
        GraphModel::NodePtr current;
        for (const auto& [id, node] : graph->GetNodes())
        {
            GraphModel::SlotPtr parent = node->GetSlot(ParentSlotId);
            if (parent == nullptr || parent->GetConnections().empty())
            {
                current = node;
                break;
            }
        }

        for (size_t index : path)
        {
            if (current == nullptr)
            {
                return nullptr;
            }

            AZStd::vector<GraphModel::NodePtr> below;
            for (const char* slotName : { ServicesSlotId, ChildrenSlotId })
            {
                if (GraphModel::SlotPtr slot = current->GetSlot(slotName); slot != nullptr)
                {
                    for (const GraphModel::ConnectionPtr& connection : slot->GetConnections())
                    {
                        if (GraphModel::NodePtr target = connection->GetTargetNode();
                            target != nullptr && target != current)
                        {
                            below.push_back(target);
                        }
                    }
                }
            }

            AZStd::stable_sort(below.begin(), below.end(),
                [&positionOf](const GraphModel::NodePtr& lhs, const GraphModel::NodePtr& rhs)
                { return positionOf(lhs).GetY() < positionOf(rhs).GetY(); });

            current = index < below.size() ? below[index] : nullptr;
        }
        return current;
    }

    AZ::Outcome<AuthoredNode, AZStd::string> MainWindow::ReadActiveGraph() const
    {
        const GraphCanvas::GraphId graphId = GetActiveGraphCanvasGraphId();
        GraphModel::GraphPtr graph = GetGraphById(graphId);
        if (graph == nullptr)
        {
            return AZ::Failure(AZStd::string("There is no open program"));
        }

        return ToAuthored(graph,
            [graphId](GraphModel::ConstNodePtr node)
            {
                AZ::Vector2 position = AZ::Vector2::CreateZero();
                GraphModelIntegration::GraphControllerRequestBus::EventResult(
                    position, graphId, &GraphModelIntegration::GraphControllerRequests::GetPosition,
                    AZStd::const_pointer_cast<GraphModel::Node>(node));
                return position;
            });
    }

    void MainWindow::Revalidate()
    {
        if (m_restoring)
        {
            return;
        }

        // Whatever the last pass painted red goes back to its own colour first.
        for (const GraphModel::NodePtr& node : m_painted)
        {
            if (auto* program = azrtti_cast<ProgramNode*>(node.get()); program != nullptr)
            {
                if (const NodeTypeDescriptor* descriptor = program->GetDescriptor(); descriptor != nullptr)
                {
                    Paint(node, ProgramNode::TitlePalette(*descriptor));
                }
            }
        }
        m_painted.clear();

        auto authored = ReadActiveGraph();
        if (!authored.IsSuccess())
        {
            // A half built graph is the normal state while editing, not something to shout about.
            statusBar()->showMessage(QString::fromUtf8(authored.GetError().c_str()));
            return;
        }

        const AZ::Name name(m_programName.empty() ? "Untitled" : m_programName.c_str());
        const ValidationResult result = Validate(authored.GetValue(), name);
        if (result.m_valid)
        {
            statusBar()->clearMessage();
            return;
        }

        statusBar()->showMessage(QString::fromUtf8(result.m_error.c_str()));
        if (GraphModel::NodePtr faulted = NodeAtPath(result.m_path); faulted != nullptr)
        {
            Paint(faulted, "ErrorNodeTitlePalette");
            m_painted.push_back(faulted);
        }
    }

    void MainWindow::LoadAuthored(const AuthoredNode& root, const AZStd::string& name, bool readOnly)
    {
        m_programName = name;
        m_readOnly = readOnly;

        const GraphCanvas::GraphId graphId = CreateNewGraph();
        GraphModel::GraphPtr graph = GetGraphById(graphId);
        if (graph == nullptr)
        {
            return;
        }

        m_restoring = true;
        AZStd::vector<PlacedNode> placed = FromAuthored(root, graph);
        AZStd::vector<GraphModel::NodePtr> added;
        added.reserve(placed.size());

        for (PlacedNode& node : placed)
        {
            AZ::Vector2 position = node.m_position;
            GraphModelIntegration::GraphControllerRequestBus::Event(graphId,
                &GraphModelIntegration::GraphControllerRequests::AddNode, node.m_node, position);
            added.push_back(node.m_node);
        }

        for (size_t i = 0; i < placed.size(); ++i)
        {
            if (placed[i].m_parent < 0)
            {
                continue;
            }

            GraphModelIntegration::GraphControllerRequestBus::Event(graphId,
                &GraphModelIntegration::GraphControllerRequests::AddConnectionBySlotId,
                added[static_cast<size_t>(placed[i].m_parent)],
                GraphModel::SlotId(placed[i].m_isService ? ServicesSlotId : ChildrenSlotId),
                added[i], GraphModel::SlotId(ParentSlotId));
        }
        m_restoring = false;

        m_undo.clear();
        m_redo.clear();
        m_undo.push_back(Capture());
        Revalidate();
    }

    void MainWindow::OnNewProgram()
    {
        AuthoredNode root;
        root.m_type = "sequence";
        m_programPath.clear();
        LoadAuthored(root, "Untitled", false);
    }

    void MainWindow::OnOpenProgram()
    {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Open GOAT Program"), QString(), tr("GOAT Programs (*.goat)"));
        if (path.isEmpty())
        {
            return;
        }

        ProgramAsset asset;
        if (!AZ::Utils::LoadObjectFromFileInPlace(path.toUtf8().constData(), asset))
        {
            QMessageBox::warning(this, tr("Open GOAT Program"), tr("That file is not a GOAT program."));
            return;
        }

        m_programPath = path.toUtf8().constData();
        LoadAuthored(asset.m_root, asset.m_name, false);
    }

    void MainWindow::OnSaveProgram()
    {
        if (m_readOnly)
        {
            QMessageBox::information(this, tr("Save GOAT Program"),
                tr("This program is authored in Lua and is shown for reference only."));
            return;
        }

        auto authored = ReadActiveGraph();
        if (!authored.IsSuccess())
        {
            QMessageBox::warning(this, tr("Save GOAT Program"),
                QString::fromUtf8(authored.GetError().c_str()));
            return;
        }

        if (m_programPath.empty())
        {
            const QString path = QFileDialog::getSaveFileName(
                this, tr("Save GOAT Program"), QString(), tr("GOAT Programs (*.goat)"));
            if (path.isEmpty())
            {
                return;
            }
            m_programPath = path.toUtf8().constData();

            AZStd::string base;
            AzFramework::StringFunc::Path::GetFileName(m_programPath.c_str(), base);
            m_programName = base;
        }

        ProgramAsset asset;
        asset.m_name = m_programName;
        asset.m_root = authored.TakeValue();

        if (!AZ::Utils::SaveObjectToFile(m_programPath, AZ::DataStream::ST_XML, &asset))
        {
            QMessageBox::warning(this, tr("Save GOAT Program"), tr("The program could not be written."));
        }
    }

    void MainWindow::OnOpenLuaProgram()
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            return;
        }

        QStringList names;
        for (const AZ::Name& name : agents->GetTreeNames())
        {
            names << QString::fromUtf8(name.GetCStr());
        }
        if (names.isEmpty())
        {
            QMessageBox::information(this, tr("Open Lua Program"),
                tr("No Lua program has been declared in this session."));
            return;
        }

        bool chosen = false;
        const QString name = QInputDialog::getItem(
            this, tr("Open Lua Program"), tr("Program:"), names, 0, false, &chosen);
        if (!chosen || name.isEmpty())
        {
            return;
        }

        auto emitted = agents->EmitProgram(AZ::Name(name.toUtf8().constData()));
        if (!emitted.IsSuccess() || emitted.GetValue() == nullptr)
        {
            QMessageBox::warning(this, tr("Open Lua Program"),
                QString::fromUtf8(emitted.IsSuccess() ? "That program is empty" : emitted.GetError().c_str()));
            return;
        }

        m_programPath.clear();
        LoadAuthored(*emitted.GetValue(), name.toUtf8().constData(), true);
    }

    QAction* MainWindow::AddFileNewAction(QMenu* menu)
    {
        QAction* action = EditorMainWindow::AddFileNewAction(menu);
        QObject::disconnect(action, nullptr, nullptr, nullptr);
        QObject::connect(action, &QAction::triggered, this, &MainWindow::OnNewProgram);
        return action;
    }

    QAction* MainWindow::AddFileOpenAction(QMenu* menu)
    {
        QAction* action = EditorMainWindow::AddFileOpenAction(menu);
        QObject::connect(action, &QAction::triggered, this, &MainWindow::OnOpenProgram);

        // The other producer of an authored program, shown but never written back.
        QAction* lua = new QAction(tr("Open &Lua Program..."), this);
        QObject::connect(lua, &QAction::triggered, this, &MainWindow::OnOpenLuaProgram);
        menu->addAction(lua);
        return action;
    }

    QAction* MainWindow::AddFileSaveAction(QMenu* menu)
    {
        QAction* action = EditorMainWindow::AddFileSaveAction(menu);
        QObject::connect(action, &QAction::triggered, this, &MainWindow::OnSaveProgram);
        return action;
    }
} // namespace GOAT::GraphEditor
