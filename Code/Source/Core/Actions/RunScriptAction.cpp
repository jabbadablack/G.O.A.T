#include <Core/Actions/RunScriptAction.h>

#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    RunScriptAction::RunScriptAction(LuaDispatch& dispatch, AgentScriptContext& scriptContext)
        : m_dispatch(dispatch)
        , m_scriptContext(scriptContext)
    {
    }

    AZ::Name RunScriptAction::GetName() const
    {
        return AZ_NAME_LITERAL("script");
    }

    void RunScriptAction::BindContext(const ActionContext& context)
    {
        m_scriptContext.Bind(context.m_agent, context.m_entity, context.m_blackboard);
    }

    void RunScriptAction::Begin(const ActionContext& context)
    {
        BindContext(context);
        m_dispatch.CallBehavior(context.m_request->m_tag, "start", context.m_agent, m_scriptContext, 0.0f);
        m_scriptContext.Unbind();
    }

    ActionResult RunScriptAction::Step(const ActionContext& context, float deltaTime)
    {
        BindContext(context);
        const ActionResult result =
            m_dispatch.CallBehavior(context.m_request->m_tag, "tick", context.m_agent, m_scriptContext, deltaTime);
        m_scriptContext.Unbind();
        return result;
    }

    void RunScriptAction::End(const ActionContext& context)
    {
        BindContext(context);
        m_dispatch.CallBehavior(context.m_request->m_tag, "stop", context.m_agent, m_scriptContext, 0.0f);
        m_scriptContext.Unbind();
    }
} // namespace GOAT
