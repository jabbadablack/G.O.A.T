#pragma once

#include <GOAT/Domain/ActionPlan.h>
#include <GOAT/Domain/PlanStore.h>
#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/PlanContext.h>
#include <GOAT/Domain/Intent.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>

namespace GOAT
{
    //! Turns an intent from the tree into a sequence of action states the agent can run.
    //! Behavior tree leaves, planners, directors and bark systems all plug in here.
    class IBackend
    {
    public:
        AZ_RTTI(IBackend, IBackendTypeId);

        virtual ~IBackend() = default;

        //! Name this backend is registered under and referenced by from Lua.
        virtual AZ::Name GetName() const = 0;

        //! Produces a plan for one intent. Returns false when this backend cannot satisfy it.
        virtual bool Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan) = 0;


        //! Releases any per agent state held for this agent.
        virtual void Release([[maybe_unused]] const PlanContext& context)
        {
        }
    };
} // namespace GOAT
