#pragma once

#include <Backends/Htn/HtnDomain.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/std/containers/fixed_vector.h>

namespace GOAT
{
    //! The blackboard as the planner pretends it will be.
    //!
    //! Only the slots the domain touches, in its own order, so a domain reading four variables
    //! copies four values however large the blackboard is.
    class WorkingState final
    {
    public:
        //! Reads the domain's slots out of the blackboard.
        void Snapshot(const HtnDomain& domain, const IBlackboardSystem& blackboard, AgentId agent);

        bool Get(const HtnDomain& domain, BlackboardKey key) const;
        void Set(const HtnDomain& domain, BlackboardKey key, bool value);

    private:
        //! The slot a key occupies, or the size when the domain never named it.
        static size_t IndexOf(const HtnDomain& domain, BlackboardKey key);

        AZStd::fixed_vector<bool, MaxDomainKeys> m_values;
    };

    //! The primitive steps one decomposition produced.
    using HtnPlanBuffer = AZStd::fixed_vector<ActionRequest, MaxPlanTasks>;

    //! Decomposes a domain into a plan of primitive steps.
    //!
    //! Depth first and total order, with no heuristic and no sorting: the hierarchy is what culls
    //! the search. Every working structure is fixed size, so planning allocates nothing.
    class HtnPlanner final
    {
    public:
        //! Plans from a task. False when no decomposition of it holds.
        bool Plan(const HtnDomain& domain, AZ::u16 rootTask, WorkingState& state, HtnPlanBuffer& outPlan) const;

    private:
        //! One compound task that was decomposed, and what to undo to take it back.
        struct Decomposed final
        {
            AZ::u16 m_task = InvalidTask;
            AZ::u16 m_method = 0;
            AZ::u16 m_toProcess = 0;
            AZ::u16 m_planned = 0;
            AZ::u16 m_undone = 0;
        };

        //! A slot the plan changed, and what it held before.
        struct Undo final
        {
            BlackboardKey m_key;
            bool m_was = false;
        };

        using TaskStack = AZStd::fixed_vector<AZ::u16, MaxPlanTasks>;
        using History = AZStd::fixed_vector<Decomposed, MaxDecomposeDepth>;
        using UndoLog = AZStd::fixed_vector<Undo, MaxPlanTasks>;

        //! True when every condition in a range holds in the working state.
        static bool Holds(
            const HtnDomain& domain, const WorkingState& state, AZ::u16 first, AZ::u16 count);
    };
} // namespace GOAT
