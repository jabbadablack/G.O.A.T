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

        //! Takes a fresh poll and reconciles it against the rows already shown.
        //!
        //! Rows are never rebuilt wholesale. A level brings its agents up over several seconds,
        //! and resetting a model of hundreds of rows on every poll while that happens costs far
        //! more than the reading of them does -- besides dropping the selection each time. So
        //! agents that are gone are removed, agents still there are updated in place, and new
        //! ones arrive a handful at a time until the list has caught up.
        void SetSnapshots(const AZStd::vector<AgentSnapshot>& snapshots);

        //! True while the table is still catching up with what the source reported.
        bool IsCatchingUp() const { return m_pending > 0; }

        const AgentSnapshot* SnapshotAt(int row) const;

        int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        int columnCount(const QModelIndex& parent = QModelIndex()) const override;
        QVariant data(const QModelIndex& index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    private:
        AZStd::vector<AgentSnapshot> m_snapshots;
        //! How many agents the last poll held that have not been added yet.
        size_t m_pending = 0;
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

        //! An agent's row was double clicked, which is the ask to open its program.
        void AgentActivated();

    private:
        AgentTableModel* m_model = nullptr;
        QTableView* m_table = nullptr;
        //! Says why the table is empty, because "no agents" and "nowhere to look" are
        //! different problems and an empty table alone reads as a broken tool.
        QLabel* m_status = nullptr;
    };
} // namespace GOAT::GraphEditor
