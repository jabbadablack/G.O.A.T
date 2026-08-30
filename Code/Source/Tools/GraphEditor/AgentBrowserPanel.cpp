#include <Tools/GraphEditor/AgentBrowserPanel.h>

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/algorithm.h>

#include <QHeaderView>
#include <QItemSelectionModel>

#include <QLabel>
#include <QTableView>
#include <QVBoxLayout>

namespace GOAT::GraphEditor
{
    namespace
    {
        //! How many newly seen agents are added to the table per poll. Enough that a level
        //! fills in about a second, few enough that no single poll is felt.
        constexpr size_t MaxArrivalsPerPoll = 24;

        enum Column
        {
            AgentColumn,
            EntityColumn,
            ProgramColumn,
            BackendColumn,
            BandColumn,
            SquadColumn,
            ActionColumn,
            StepColumn,
            ColumnCount
        };

        const char* ColumnTitle(int column)
        {
            switch (column)
            {
            case AgentColumn:
                return "Agent";
            case EntityColumn:
                return "Entity";
            case ProgramColumn:
                return "Program";
            case BackendColumn:
                return "Backend";
            case BandColumn:
                return "Band";
            case SquadColumn:
                return "Squad";
            case ActionColumn:
                return "Action";
            case StepColumn:
                return "Step";
            default:
                return "";
            }
        }
    } // namespace

    AgentTableModel::AgentTableModel(QObject* parent)
        : QAbstractTableModel(parent)
    {
    }

    void AgentTableModel::SetSnapshots(const AZStd::vector<AgentSnapshot>& snapshots)
    {
        AZStd::unordered_map<AgentId, const AgentSnapshot*> incoming;
        incoming.reserve(snapshots.size());
        for (const AgentSnapshot& snapshot : snapshots)
        {
            incoming[snapshot.GetAgent()] = &snapshot;
        }

        // Gone first, walking backwards so a removal never shifts a row still to be examined.
        for (int row = aznumeric_cast<int>(m_snapshots.size()) - 1; row >= 0; --row)
        {
            if (incoming.find(m_snapshots[row].GetAgent()) == incoming.end())
            {
                beginRemoveRows(QModelIndex(), row, row);
                m_snapshots.erase(m_snapshots.begin() + row);
                endRemoveRows();
            }
        }

        // Then the ones still here, in place.
        AZStd::unordered_set<AgentId> shown;
        shown.reserve(m_snapshots.size());
        for (AgentSnapshot& row : m_snapshots)
        {
            row = *incoming[row.GetAgent()];
            shown.insert(row.GetAgent());
        }
        if (!m_snapshots.empty())
        {
            emit dataChanged(index(0, 0),
                index(aznumeric_cast<int>(m_snapshots.size()) - 1, ColumnCount - 1));
        }

        // Then whatever is new, a few at a time. A level brings its agents up over several
        // seconds, and taking them in a stream keeps the editor answering while it does.
        AZStd::vector<const AgentSnapshot*> arriving;
        for (const AgentSnapshot& snapshot : snapshots)
        {
            if (shown.find(snapshot.GetAgent()) == shown.end())
            {
                arriving.push_back(&snapshot);
            }
        }

        m_pending = arriving.size();
        if (arriving.empty())
        {
            return;
        }

        const size_t taking = AZStd::min(arriving.size(), MaxArrivalsPerPoll);
        const int first = aznumeric_cast<int>(m_snapshots.size());
        beginInsertRows(QModelIndex(), first, first + aznumeric_cast<int>(taking) - 1);
        for (size_t i = 0; i < taking; ++i)
        {
            m_snapshots.push_back(*arriving[i]);
        }
        endInsertRows();
        m_pending -= taking;
    }

    const AgentSnapshot* AgentTableModel::SnapshotAt(int row) const
    {
        return row >= 0 && row < aznumeric_cast<int>(m_snapshots.size()) ? &m_snapshots[row] : nullptr;
    }

    int AgentTableModel::rowCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : aznumeric_cast<int>(m_snapshots.size());
    }

    int AgentTableModel::columnCount(const QModelIndex& parent) const
    {
        return parent.isValid() ? 0 : ColumnCount;
    }

    QVariant AgentTableModel::data(const QModelIndex& index, int role) const
    {
        if (role != Qt::DisplayRole)
        {
            return QVariant();
        }

        const AgentSnapshot* snapshot = SnapshotAt(index.row());
        if (snapshot == nullptr)
        {
            return QVariant();
        }

        switch (index.column())
        {
        case AgentColumn:
            return QVariant(snapshot->m_agentIndex);
        case EntityColumn:
            return QString(snapshot->m_entity.ToString().c_str());
        case ProgramColumn:
            return QString(snapshot->m_program.GetCStr());
        case BackendColumn:
            return QString(snapshot->m_backend.GetCStr());
        case BandColumn:
            return QVariant(static_cast<uint>(snapshot->m_band));
        case SquadColumn:
            return QString(snapshot->m_squad.GetCStr());
        case ActionColumn:
            return QString(snapshot->m_action.GetCStr());
        case StepColumn:
            return snapshot->m_planSize > 0
                ? QString("%1 of %2").arg(snapshot->m_step + 1).arg(snapshot->m_planSize)
                : QString("-");
        default:
            return QVariant();
        }
    }

    QVariant AgentTableModel::headerData(int section, Qt::Orientation orientation, int role) const
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        {
            return QVariant();
        }
        return QString(ColumnTitle(section));
    }

    AgentBrowserPanel::AgentBrowserPanel(QWidget* parent)
        : QDockWidget(parent)
    {
        setWindowTitle("Agents");
        setObjectName("GOATAgentBrowser");

        auto* contents = new QWidget(this);
        auto* layout = new QVBoxLayout(contents);
        layout->setContentsMargins(0, 0, 0, 0);

        m_status = new QLabel(contents);
        m_status->setWordWrap(true);
        m_status->setContentsMargins(6, 4, 6, 4);
        layout->addWidget(m_status);

        m_model = new AgentTableModel(this);
        m_table = new QTableView(contents);
        m_table->setModel(m_model);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->verticalHeader()->setVisible(false);
        m_table->horizontalHeader()->setStretchLastSection(true);
        layout->addWidget(m_table);

        setWidget(contents);

        connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this]()
            {
                emit SelectedAgentChanged();
            });

        connect(m_table, &QTableView::doubleClicked, this,
            [this](const QModelIndex&)
            {
                emit AgentActivated();
            });
    }

    void AgentBrowserPanel::SetSnapshots(
        const AZStd::vector<AgentSnapshot>& snapshots, const AZStd::string& target)
    {
        // No reset happens in here, so the selection simply survives.
        m_model->SetSnapshots(snapshots);

        QString status = snapshots.empty() ? QString(target.c_str())
                                           : QString("Watching %1").arg(target.c_str());
        if (m_model->IsCatchingUp())
        {
            status += QString(" - listing%1").arg(QString("..."));
        }
        m_status->setText(status);
    }

    const AgentSnapshot* AgentBrowserPanel::GetSelected() const
    {
        const QModelIndexList selected = m_table->selectionModel()->selectedRows();
        return selected.isEmpty() ? nullptr : m_model->SnapshotAt(selected.front().row());
    }
} // namespace GOAT::GraphEditor
