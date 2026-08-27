#pragma once

#include <Backends/BehaviorTree/DecisionCursor.h>

#include <GOAT/Domain/DecisionProgram.h>

#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/fixed_vector.h>

namespace GOAT
{
    //! Services that came due in one tick. Bounded by the slots a tree may have, so collecting
    //! them costs no allocation: they are gathered and run inside one call and never outlive it.
    using DueServices = AZStd::fixed_vector<AZ::u32, MaxCursorSlots>;
    //! Works out which services should run this tick.
    //! A service is in scope while the agent's active leaf sits inside the composite it is
    //! attached to, which is what makes a service the sanctioned place to do periodic sensing.
    class ServiceTracker final
    {
    public:
        //! Collects the services that are both in scope and past their interval, and
        //! schedules each collected service's next run.
        void CollectDue(
            const DecisionProgram& program, DecisionCursor& cursor, DueServices& outServices) const;
    };
} // namespace GOAT
