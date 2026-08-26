#include <Core/Scripting/LuaBackend.h>

namespace GOAT
{
    LuaBackend::LuaBackend(AZ::Name name, LuaDispatch& dispatch, AgentScriptContext& scriptContext)
        : m_name(AZStd::move(name))
        , m_dispatch(dispatch)
        , m_scriptContext(scriptContext)
    {
    }

    AZ::Name LuaBackend::GetName() const
    {
        return m_name;
    }

    bool LuaBackend::Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan)
    {
        m_scriptContext.Bind(context.m_agent, context.m_entity, context.m_blackboard);
        const ActionPlan* planned = m_dispatch.CallBackendPlan(m_name, intent.m_goal, context.m_agent, m_scriptContext);
        m_scriptContext.Unbind();

        if (planned == nullptr || planned->IsEmpty())
        {
            return false;
        }

        outPlan = *planned;
        return true;
    }
} // namespace GOAT
