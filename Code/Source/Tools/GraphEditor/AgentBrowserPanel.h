#pragma once

#include <GOAT/Domain/AgentDebug.h>

#include <AzCore/std/containers/vector.h>

#include <QAbstractTableModel>
#include <QDockWidget>

class QLabel;
class QTableView;

namespace GOAT::GraphEditor
{
    //! The agents a debug source is reporting, as rows.
    class AgentTableModel final
        : public QAbstractTableModel
    {
        Q_OBJECT // AUTOMOC
    public:
        explicit AgentTableModel(QObject* parent = nullptr);

        //! Takes a fresh poll. The rows are only rebuilt when the set of agents has actually
        //! changed, because resetting a model drops the selection, and at ten polls a second
        //! that would make an agent impossible to keep hold of.
        void SetSnapshots(const AZStd::vector<AgentSnapshot>& snapshots);

        const AgentSnapshot* SnapshotAt(int row) const;

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    private:
        //! True when both lists name the same agents in the same order.
        bool SameAgents(const AZStd::vector<AgentSnapshot>& snapshots) const;

        AZStd::vector<AgentSnapshot> m_snapshots;
    };

    //! Lists the agents that are running, so one can be picked to watch.
    class AgentBrowserPanel final
        : public QDockWidget
    {
        Q_OBJECT // AUTOMOC
    public:
        explicit AgentBrowserPanel(QWidget* parent = nullptr);

        //! Shows a poll, and says what the source is watching when there is nothing to show.
        void SetSnapshots(const AZStd::vector<AgentSnapshot>& snapshots, const AZStd::string& target);

        //! The agent whose row is selected, or null when none is.
        const AgentSnapshot* GetSelected() const;

    signals:
        //! A different agent was picked, or the picked one went away.
        void SelectedAgentChanged();

    private:
        AgentTableModel* m_model = nullptr;
        QTableView* m_table = nullptr;
        //! Says why the table is empty, because "no agents" and "nowhere to look" are
        //! different problems and an empty table alone reads as a broken tool.
        QLabel* m_status = nullptr;
    };
} // namespace GOAT::GraphEditor
