#include <Core/Scripting/LuaNodeScripting.h>

namespace GOAT
{
    LuaNodeScripting::LuaNodeScripting(LuaDispatch& dispatch, AgentScriptContext& scriptContext)
        : m_dispatch(dispatch)
        , m_scriptContext(scriptContext)
    {
    }

    int LuaNodeScripting::BeginComposite(
        const AZ::Name& behavior, const PlanContext& context, NodeIndex node, int childCount, ActionResult& outResult)
    {
        m_scriptContext.Bind(context.m_agent, context.m_entity, context.m_blackboard);
        const int child =
            m_dispatch.CallFlowBegin(behavior, context.m_agent, m_scriptContext, node, childCount, outResult);
        m_scriptContext.Unbind();
        return child;
    }

    int LuaNodeScripting::AdvanceComposite(
        const AZ::Name& behavior,
        const PlanContext& context,
        NodeIndex node,
        int childIndex,
        ActionResult childResult,
        ActionResult& outResult)
    {
        m_scriptContext.Bind(context.m_agent, context.m_entity, context.m_blackboard);
        const int child = m_dispatch.CallFlowAdvance(
            behavior, context.m_agent, m_scriptContext, node, childIndex, childResult, outResult);
        m_scriptContext.Unbind();
        return child;
    }

    ActionResult LuaNodeScripting::FilterDecorator(
        const AZ::Name& behavior, const PlanContext& context, NodeIndex node, ActionResult childResult)
    {
        m_scriptContext.Bind(context.m_agent, context.m_entity, context.m_blackboard);
        const ActionResult result =
            m_dispatch.CallFlowFilter(behavior, context.m_agent, m_scriptContext, node, childResult);
        m_scriptContext.Unbind();
        return result;
    }
} // namespace GOAT
