#pragma once

#include <GOAT/Domain/DecisionProgram.h>
#include <GOAT/Interfaces/IBackend.h>

namespace GOAT
{
    //! Evaluates a condition or comparison node against the blackboard.
    //! Shared by the walker and the guard evaluator so both read a guard the same way.
    bool EvaluateNodePredicate(const DecisionNode& node, const PlanContext& context);

    //! Evaluates a whole subtree of instantaneous nodes and reports what it decided.
    //!
    //! Used for a parallel's background branch, which the compiler has already proved contains
    //! nothing that could emit an action. That is what makes this a plain recursive walk with no
    //! cursor: there is no running state to remember, because nothing here can take time.
    ActionResult EvaluateSubtree(const DecisionProgram& program, NodeIndex index, const PlanContext& context);
} // namespace GOAT
