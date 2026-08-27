#pragma once

#include <Backends/BehaviorTree/DecisionCursor.h>

#include <GOAT/Domain/DecisionProgram.h>
#include <GOAT/Interfaces/IBackend.h>

namespace GOAT
{
    //! What a guard that changed should do to the branch an agent is running.
    enum class AbortAction : AZ::u8
    {
        None,   //!< Nothing changed that affects this agent.
        Fail,   //!< A guard around the running branch stopped holding.
        Restart //!< A higher priority guard started holding, so the walk moves there.
    };

    //! The interruption a re-check decided on.
    struct AbortDecision final
    {
        AbortAction m_action = AbortAction::None;
        //! The guard node that caused it.
        NodeIndex m_node = InvalidNodeIndex;
    };

    //! Re-checks the guards a tree declared, using Unreal's four observer abort modes.
    //! Only called when a watched blackboard key actually changed, which is what keeps
    //! an idle agent from evaluating any condition at all.
    class GuardEvaluator final
    {
    public:
        AbortDecision Evaluate(
            const DecisionProgram& program, const DecisionCursor& cursor, const PlanContext& context) const;

    private:
        //! Re-checks the background branch of every parallel whose main branch is running.
        AbortDecision EvaluateParallels(
            const DecisionProgram& program, NodeIndex leaf, const PlanContext& context) const;
    };
} // namespace GOAT
