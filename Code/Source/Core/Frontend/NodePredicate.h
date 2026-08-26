#pragma once

#include <GOAT/Domain/DecisionProgram.h>
#include <GOAT/Interfaces/IBackend.h>

namespace GOAT
{
    //! Evaluates a condition or comparison node against the blackboard.
    //! Shared by the walker and the guard evaluator so both read a guard the same way.
    bool EvaluateNodePredicate(const DecisionNode& node, const PlanContext& context);
} // namespace GOAT
