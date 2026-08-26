#pragma once

#include <Core/Scripting/AgentScriptContext.h>
#include <Core/Scripting/LuaDispatch.h>

#include <GOAT/Interfaces/IActionState.h>

#include <AzCore/Memory/SystemAllocator.h>

namespace GOAT
{
    //! Runs the Lua behaviour named by the action request.
    //! The behaviour's start, tick and stop phases map onto this state's lifecycle.
    class RunScriptAction final
        : public IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(RunScriptAction, AZ::SystemAllocator);

        //! Both references outlive this state; the script context is deliberately shared
        //! and stable, because Lua receives a raw pointer to it during a call.
        RunScriptAction(LuaDispatch& dispatch, AgentScriptContext& scriptContext);

        AZ::Name GetName() const override;
        void Begin(const ActionContext& context) override;
        ActionResult Step(const ActionContext& context, float deltaTime) override;
        void End(const ActionContext& context) override;

    private:
        //! Points the shared script context at the agent this call is for.
        void BindContext(const ActionContext& context);

        LuaDispatch& m_dispatch;
        AgentScriptContext& m_scriptContext;
    };
} // namespace GOAT
