#pragma once

#include <Core/Scripting/AgentScriptContext.h>
#include <Core/Scripting/LuaDispatch.h>

#include <GOAT/Interfaces/IBackend.h>
#include <GOAT/Interfaces/INodeScripting.h>

namespace GOAT
{
    //! Routes the walker's control flow questions to Lua.
    //! The walker only knows the interface, so a tree using user written control flow costs
    //! nothing when no script defines any.
    class LuaNodeScripting final
        : public INodeScripting
    {
    public:
        LuaNodeScripting(LuaDispatch& dispatch, AgentScriptContext& scriptContext);

        int BeginComposite(
            const AZ::Name& behavior,
            const PlanContext& context,
            NodeIndex node,
            int childCount,
            ActionResult& outResult) override;

        int AdvanceComposite(
            const AZ::Name& behavior,
            const PlanContext& context,
            NodeIndex node,
            int childIndex,
            ActionResult childResult,
            ActionResult& outResult) override;

        ActionResult FilterDecorator(
            const AZ::Name& behavior, const PlanContext& context, NodeIndex node, ActionResult childResult) override;

    private:
        LuaDispatch& m_dispatch;
        AgentScriptContext& m_scriptContext;
    };
} // namespace GOAT
