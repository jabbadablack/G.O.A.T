#include <Tools/GraphEditor/MainWindow.h>
#include <Tools/GraphEditor/GraphContext.h>
#include <Tools/GraphEditor/ProgramFile.h>
#include <Tools/GraphEditor/ProgramGraphSerializer.h>
#include <Tools/GraphEditor/ProgramLayout.h>
#include <Tools/GraphEditor/ProgramNode.h>
#include <Tools/GraphEditor/ProgramNodePaletteItem.h>
#include <Tools/GraphEditor/ProgramValidator.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <GraphCanvas/Components/GeometryBus.h>
#include <GraphCanvas/Components/Nodes/NodeTitleBus.h>
#include <GraphCanvas/Components/VisualBus.h>
#include <GraphCanvas/Widgets/NodePalette/TreeItems/IconDecoratedNodePaletteTreeItem.h>
#include <GraphCanvas/Widgets/NodePalette/TreeItems/NodePaletteTreeItem.h>
#include <GraphModel/GraphModelBus.h>
#include <GraphModel/Model/Connection.h>
#include <GraphModel/Model/Graph.h>
#include <GraphModel/Model/Slot.h>

#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/map.h>
#include <AzCore/std/sort.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>

#include <QAction>
#include <QGraphicsItem>
#include <QTimer>
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

        //! What a node is assumed to be when it cannot be measured, so a failed measurement
        //! spaces nodes too generously rather than piling them up.
        constexpr float MinimumNodeWidth = 200.0f;
        constexpr float MinimumNodeHeight = 120.0f;

        //! Holds a flag for as long as it is in scope, so an early return still clears it.
        class Holding final
        {
        public:
            explicit Holding(bool& flag)
                : m_flag(flag)
                , m_was(flag)
            {
                m_flag = true;
            }
            ~Holding()
            {
                m_flag = m_was;
            }

        private:
            bool& m_flag;
            bool m_was;
        };

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
        SetDropAreaText("Create a GOAT program in the Asset Browser, or open one from the File menu.");
        GOATProgramEditorRequestBus::Handler::BusConnect();
    }

    MainWindow::~MainWindow()
    {
        GOATProgramEditorRequestBus::Handler::BusDisconnect();
    }

    void MainWindow::OpenProgram(const AZStd::string& fullPath)
    {
        ProgramAsset asset;
        if (!LoadProgramFile(fullPath, asset))
        {
            QMessageBox::warning(this, tr("Open GOAT Program"),
                tr("%1 could not be read as a GOAT program.").arg(QString::fromUtf8(fullPath.c_str())));
            return;
        }

        m_programPath = fullPath;
        LoadAuthored(asset.m_root, asset.m_name, false);
    }

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
        const GraphCanvas::GraphId* graphId =
            GraphModelIntegration::GraphControllerNotificationBus::GetCurrentBusId();

        // Every word is drawn in its own paradigm's and kind's colour, which is the only cue
        // saying what a node is once the palette is closed.
        if (auto* program = azrtti_cast<ProgramNode*>(node.get()); graphId != nullptr && program != nullptr)
        {
            if (const NodeTypeDescriptor* descriptor = program->GetDescriptor(); descriptor != nullptr)
            {
                Paint(*graphId, node, ProgramNode::TitlePalette(*descriptor));
            }
        }
        QueueRevalidate();
    }

    void MainWindow::OnGraphModelGraphModified([[maybe_unused]] GraphModel::NodePtr node)
    {
        QueueRevalidate();
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
        QueueRestore(m_undo.back());
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
        QueueRestore(next);
    }

    void MainWindow::QueueRestore(const Snapshot& snapshot)
    {
        // Restoring destroys the graph controller, and this is called from inside one of its own
        // calls, so it has to wait until that call has returned.
        QTimer::singleShot(0, this, [this, snapshot]() { Restore(snapshot); });
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

        QueueRevalidate();
    }

    void MainWindow::Paint(
        const GraphCanvas::GraphId& graphId, GraphModel::NodePtr node, const char* palette)
    {
        const Holding painting(m_validating);

        GraphCanvas::NodeId nodeId;
        GraphModelIntegration::GraphControllerRequestBus::EventResult(
            nodeId, graphId, &GraphModelIntegration::GraphControllerRequests::GetNodeIdByNode, node);
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

    void MainWindow::QueueRevalidate()
    {
        if (m_restoring || m_validating || m_revalidateQueued)
        {
            return;
        }

        m_revalidateQueued = true;
        QTimer::singleShot(0, this,
            [this]()
            {
                m_revalidateQueued = false;
                Revalidate();
            });
    }

    void MainWindow::Revalidate()
    {
        if (m_restoring || m_validating)
        {
            return;
        }

        const Holding validating(m_validating);

        const GraphCanvas::GraphId graphId = GetActiveGraphCanvasGraphId();
        if (GetGraphById(graphId) == nullptr)
        {
            // Nothing is open, which is not a fault to report.
            statusBar()->clearMessage();
            return;
        }

        // Whatever the last pass painted red goes back to its own colour first.
        for (const GraphModel::NodePtr& node : m_painted)
        {
            if (auto* program = azrtti_cast<ProgramNode*>(node.get()); program != nullptr)
            {
                if (const NodeTypeDescriptor* descriptor = program->GetDescriptor(); descriptor != nullptr)
                {
                    Paint(graphId, node, ProgramNode::TitlePalette(*descriptor));
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
            Paint(graphId, faulted, "ErrorNodeTitlePalette");
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

        // Read from what was authored, not from what was flattened: flattening fills a guessed
        // position in for every node that had none, so by then they all look placed.
        const bool authoredLayout = HasAuthoredLayout(root);
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

        if (authoredLayout)
        {
            m_undo.push_back(Capture());
            QueueRevalidate();
            return;
        }

        // A node has no size until Qt has laid its widget out, so measuring it in this same call
        // reads zero for every one of them and the whole program collapses into one column.
        QTimer::singleShot(0, this,
            [this, graphId, placed, added]()
            {
                {
                    const Holding restoring(m_restoring);
                    LayoutByMeasuredSize(graphId, placed, added);
                }
                m_undo.clear();
                m_undo.push_back(Capture());
                QueueRevalidate();
            });
    }

    void MainWindow::LayoutByMeasuredSize(const GraphCanvas::GraphId& graphId,
        const AZStd::vector<PlacedNode>& placed, const AZStd::vector<GraphModel::NodePtr>& added) const
    {
        const size_t count = placed.size();
        if (count == 0 || added.size() != count)
        {
            return;
        }

        AZStd::vector<GraphCanvas::NodeId> nodeIds(count);
        AZStd::vector<LayoutNode> measured(count);

        for (size_t i = 0; i < count; ++i)
        {
            GraphModelIntegration::GraphControllerRequestBus::EventResult(
                nodeIds[i], graphId, &GraphModelIntegration::GraphControllerRequests::GetNodeIdByNode, added[i]);

            QGraphicsItem* item = nullptr;
            GraphCanvas::SceneMemberUIRequestBus::EventResult(
                item, nodeIds[i], &GraphCanvas::SceneMemberUIRequests::GetRootGraphicsItem);

            measured[i].m_parent = placed[i].m_parent;
            if (item != nullptr)
            {
                const QRectF bounds = item->boundingRect();
                measured[i].m_width = static_cast<float>(bounds.width());
                measured[i].m_height = static_cast<float>(bounds.height());
            }

            // Whatever went wrong with measuring, two nodes must never land on each other.
            measured[i].m_width = AZStd::max(measured[i].m_width, MinimumNodeWidth);
            measured[i].m_height = AZStd::max(measured[i].m_height, MinimumNodeHeight);
        }

        const AZStd::vector<AZ::Vector2> positions = LayoutProgram(measured);
        for (size_t i = 0; i < count; ++i)
        {
            GraphCanvas::GeometryRequestBus::Event(
                nodeIds[i], &GraphCanvas::GeometryRequests::SetPosition, positions[i]);
        }
    }

    void MainWindow::OnNewProgram()
    {
        m_programPath.clear();
        LoadAuthored(DefaultRoot(), "Untitled", false);
    }

    void MainWindow::OnOpenProgram()
    {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Open GOAT Program"), QString(), tr("GOAT Programs (*.goat)"));
        if (path.isEmpty())
        {
            return;
        }

        OpenProgram(path.toUtf8().constData());
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

        if (!SaveProgramFile(m_programPath, asset))
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
