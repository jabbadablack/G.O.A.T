#pragma once

#include <Core/Application/ActionStateRegistry.h>
#include <Core/Application/AgentRecord.h>
#include <Core/Application/BackendRegistry.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>
#include <GOAT/Interfaces/INodeScripting.h>

namespace GOAT
{
    //! Runs one tick of the pipeline for one agent: whatever its backend decides, the running
    //! action, and a new decision when that action finishes.
    class AgentRuntime final
    {
    public:
        AgentRuntime(
            IBlackboardSystem& blackboard,
            const ActionStateRegistry& actions,
            const BackendRegistry& backends,
            INodeScripting& scripting,
            PlanStore& planStore);

        //! Tells every backend an agent is gone. Called when it unregisters, which is the one
        //! moment per agent state can be dropped without guessing whether a plan will resume.
        void ReleaseAgent(AgentRecord& agent);

        //! Advances one agent by a delta time.
        void Tick(AgentRecord& agent, float deltaTime);

        //! Installs what applies a deferred tree switch. Resolving a name to a program needs the
        //! system component, which owns the compiled programs.
        void SetTreeSwitchHandler(AZStd::function<void(AgentRecord&)> handler)
        {
            m_applySwitch = AZStd::move(handler);
        }

        //! Ends whatever an agent is running and gives back what that action held.
        void AbortAgent(AgentRecord& agent);

        //! Builds the context a backend receives.
        PlanContext MakePlanContext(AgentRecord& agent) const;

    private:
        //! Builds the context an action state receives.
        ActionContext MakeActionContext(AgentRecord& agent) const;

        IBlackboardSystem& m_blackboard;
        const ActionStateRegistry& m_actions;
        const BackendRegistry& m_backends;
        INodeScripting& m_scripting;
        PlanStore& m_planStore;
        AZStd::function<void(AgentRecord&)> m_applySwitch;
    };
} // namespace GOAT
