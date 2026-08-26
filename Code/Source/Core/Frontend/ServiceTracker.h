#pragma once

#include <Core/Frontend/DecisionCursor.h>

#include <GOAT/Domain/DecisionProgram.h>

#include <AzCore/std/algorithm.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Works out which services should run this tick.
    //! A service is in scope while the agent's active leaf sits inside the composite it is
    //! attached to, which is what makes a service the sanctioned place to do periodic sensing.
    class ServiceTracker final
    {
    public:
        //! Collects the services that are both in scope and past their interval, and
        //! schedules each collected service's next run.
        void CollectDue(
            const DecisionProgram& program, DecisionCursor& cursor, AZStd::vector<AZ::u32>& outServices) const;
    };
} // namespace GOAT
