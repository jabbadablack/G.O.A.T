#pragma once

#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/AgentId.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/containers/array.h>

namespace GOAT
{
    class IBlackboardSystem;

    //! Per agent bookkeeping an action state may use while it runs.
    //! One instance of an action state serves every agent, so all mutable state lives here.
    using ActionScratch = AZStd::array<AZ::u8, 32>;

    //! Everything an action state may reach while running for one agent.
    struct ActionContext final
    {
        //! The agent this action is running for.
        AgentId m_agent;
        //! The entity the agent drives.
        AZ::EntityId m_entity;
        //! Shared data, for reading action parameters and writing outcomes.
        IBlackboardSystem* m_blackboard = nullptr;
        //! Parameters of the action being run.
        const ActionRequest* m_request = nullptr;
        //! Scratch owned by this agent's state machine, zeroed before Begin.
        ActionScratch* m_scratch = nullptr;
    };

    //! One executable verb. Modules and backends register these to extend the vocabulary.
    //! A single instance serves every agent, so implementations must keep state in the context.
    class IActionState
    {
    public:
        AZ_RTTI(IActionState, IActionStateTypeId);

        virtual ~IActionState() = default;

        //! Name this verb is registered under and referenced by from Lua.
        virtual AZ::Name GetName() const = 0;

        //! Begins the action for one agent. Optional: a verb with nothing to set up says nothing,
        //! rather than every such verb carrying an empty body to satisfy the interface.
        virtual void Begin([[maybe_unused]] const ActionContext& context)
        {
        }

        //! True when this verb touches nothing but its own context: its scratch, and the
        //! blackboard of the agent it is running for.
        //!
        //! The default is no, because the honest constraint is wider than Lua. A verb that
        //! reaches an EBus, an asset, a transform or a script is talking to something outside
        //! the gem, and nothing out there promises to be reentrant -- MoveToAction writes a
        //! transform, AnimateAction broadcasts on the asset catalog. Answering yes is a claim
        //! the verb's author makes deliberately, not one inferred on their behalf.
        virtual bool IsParallelSafe() const
        {
            return false;
        }

        //! Advances the action. The agent stays in this state while it returns Running.
        virtual ActionResult Step(const ActionContext& context, float deltaTime) = 0;

        //! Ends the action, whether it completed or was aborted. Optional, like Begin.
        virtual void End([[maybe_unused]] const ActionContext& context)
        {
        }
    };
} // namespace GOAT
