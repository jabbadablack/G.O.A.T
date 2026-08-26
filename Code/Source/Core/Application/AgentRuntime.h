#pragma once

#include <Core/Application/ActionStateRegistry.h>
#include <Core/Application/AgentRecord.h>
#include <Core/Application/BackendRegistry.h>
#include <Core/Frontend/GuardEvaluator.h>
#include <Core/Frontend/ServiceTracker.h>
#include <Core/Frontend/TreeWalker.h>
#include <Core/Scripting/AgentScriptContext.h>
#include <Core/Scripting/LuaDispatch.h>

#include <GOAT/Interfaces/INodeScripting.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

namespace GOAT
{
    //! Runs one tick of the pipeline for one agent: guards, services, the running action,
    //! and a re-plan when the action finishes.
    class AgentRuntime final
    {
    public:
        AgentRuntime(
            IBlackboardSystem& blackboard,
            const ActionStateRegistry& actions,
            const BackendRegistry& backends,
            IBackend& directBackend,
            LuaDispatch& dispatch,
            AgentScriptContext& scriptContext,
            INodeScripting& scripting);

        //! Advances one agent by a delta time.
        void Tick(AgentRecord& agent, float deltaTime);

    private:
        //! Re-checks the guards that a changed blackboard slot could have affected.
        //! Returns true when the running action was interrupted.
        bool ApplyGuards(AgentRecord& agent, const PlanContext& planContext, WalkStep& outStep, bool& outHaveStep);

        //! Runs the services whose subtree the agent is currently inside.
        void TickServices(AgentRecord& agent, float deltaTime);

        //! Turns an intent into a plan and loads it into the state machine.
        //! Returns false when no backend could satisfy the intent.
        bool StartPlan(AgentRecord& agent, const PlanContext& planContext, const Intent& intent);

        //! Builds the context an action state receives.
        ActionContext MakeActionContext(AgentRecord& agent) const;

        //! Builds the context a backend receives.
        PlanContext MakePlanContext(AgentRecord& agent) const;

        IBlackboardSystem& m_blackboard;
        const ActionStateRegistry& m_actions;
        const BackendRegistry& m_backends;
        IBackend& m_directBackend;
        LuaDispatch& m_dispatch;
        AgentScriptContext& m_scriptContext;
        INodeScripting& m_scripting;
        TreeWalker m_walker;
        GuardEvaluator m_guards;
        ServiceTracker m_services;
    };
} // namespace GOAT
