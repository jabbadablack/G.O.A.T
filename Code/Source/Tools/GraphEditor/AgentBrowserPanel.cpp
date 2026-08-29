#include <Tools/GraphEditor/AgentBrowserPanel.h>

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QSignalBlocker>
#include <QLabel>
#include <QTableView>
#include <QVBoxLayout>

namespace GOAT::GraphEditor
{
    namespace
    {
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

    bool AgentTableModel::SameAgents(const AZStd::vector<AgentSnapshot>& snapshots) const
    {
        if (snapshots.size() != m_snapshots.size())
        {
            return false;
        }
        for (size_t i = 0; i < snapshots.size(); ++i)
        {
            if (snapshots[i].GetAgent() != m_snapshots[i].GetAgent())
            {
                return false;
            }
        }
        return true;
    }

    void AgentTableModel::SetSnapshots(const AZStd::vector<AgentSnapshot>& snapshots)
    {
        if (SameAgents(snapshots))
        {
            m_snapshots = snapshots;
            if (!m_snapshots.empty())
            {
                emit dataChanged(index(0, 0),
                    index(aznumeric_cast<int>(m_snapshots.size()) - 1, ColumnCount - 1));
            }
            return;
        }

        beginResetModel();
        m_snapshots = snapshots;
        endResetModel();
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
    }

    void AgentBrowserPanel::SetSnapshots(
        const AZStd::vector<AgentSnapshot>& snapshots, const AZStd::string& target)
    {
        const AgentSnapshot* wasSelected = GetSelected();
        const AgentId held = wasSelected != nullptr ? wasSelected->GetAgent() : AgentId();

        m_model->SetSnapshots(snapshots);

        // A reset drops the selection, so put it back on the same agent rather than on
        // whatever row now happens to sit where it used to. Silently: restoring a selection is
        // not the user picking one, and agents register in a stream as a level starts, so this
        // runs on most polls. Announced, it would look like ten choices a second.
        if (!held.IsNull() && GetSelected() == nullptr)
        {
            const QSignalBlocker quiet(m_table->selectionModel());
            for (int row = 0; row < m_model->rowCount(); ++row)
            {
                if (m_model->SnapshotAt(row)->GetAgent() == held)
                {
                    m_table->selectRow(row);
                    break;
                }
            }
        }

        m_status->setText(snapshots.empty() ? QString(target.c_str())
                                            : QString("Watching %1").arg(target.c_str()));

        // Only worth announcing when the agent that was being watched has actually gone.
        if (!held.IsNull() && GetSelected() == nullptr)
        {
            emit SelectedAgentChanged();
        }
    }

    const AgentSnapshot* AgentBrowserPanel::GetSelected() const
    {
        const QModelIndexList selected = m_table->selectionModel()->selectedRows();
        return selected.isEmpty() ? nullptr : m_model->SnapshotAt(selected.front().row());
    }
} // namespace GOAT::GraphEditor
