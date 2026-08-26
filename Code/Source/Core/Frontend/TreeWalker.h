#pragma once

#include <Core/Frontend/DecisionCursor.h>

#include <GOAT/Domain/DecisionProgram.h>
#include <GOAT/Domain/Intent.h>
#include <GOAT/Interfaces/IBackend.h>

namespace GOAT
{
    //! What the walker produced this step.
    enum class WalkOutcome : AZ::u8
    {
        Intent,  //!< An intent is ready to hand to a backend.
        Finished //!< The tree ran out of work; the result says how it ended.
    };

    //! One step of a walk.
    struct WalkStep final
    {
        WalkOutcome m_outcome = WalkOutcome::Finished;
        //! Valid only when the outcome is Intent.
        Intent m_intent;
        //! Valid only when the outcome is Finished.
        ActionResult m_result = ActionResult::Success;
    };

    //! Walks a compiled tree for one agent, producing one intent at a time.
    //! The walk is iterative so a wide branch cannot grow the call stack.
    class TreeWalker final
    {
    public:
        //! Rewinds to the root and produces the first intent.
        WalkStep Begin(const DecisionProgram& program, DecisionCursor& cursor, const PlanContext& context) const;

        //! Reports how the running intent's plan ended and produces the next intent.
        WalkStep Advance(
            const DecisionProgram& program,
            DecisionCursor& cursor,
            const PlanContext& context,
            ActionResult lastResult) const;

    private:
        //! Runs the walk from a node, either descending into it or bubbling a result out of it.
        WalkStep Run(
            const DecisionProgram& program,
            DecisionCursor& cursor,
            const PlanContext& context,
            NodeIndex node,
            bool bubbling,
            ActionResult result) const;

        //! Evaluates a condition or comparison node against the blackboard.
        bool EvaluatePredicate(const DecisionNode& node, const PlanContext& context) const;

        //! Builds the intent a leaf node emits.
        Intent MakeIntent(const DecisionNode& node, NodeIndex index) const;
    };
} // namespace GOAT
