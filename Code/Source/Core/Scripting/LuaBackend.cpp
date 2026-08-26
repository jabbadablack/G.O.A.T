#include <Core/Scripting/LuaBackend.h>

#include <AzCore/Debug/Trace.h>

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
        AZ_Assert(!m_name.IsEmpty(), "A Lua backend is always registered under a name");
        AZ_Assert(context.m_blackboard != nullptr, "Planning always runs with a blackboard");

        m_scriptContext.Bind(context.m_agent, context.m_entity, context.m_blackboard);
        const ActionPlan* planned = m_dispatch.CallBackendPlan(m_name, intent.m_goal, context.m_agent, m_scriptContext);
        m_scriptContext.Unbind();

        if (planned == nullptr || planned->IsEmpty())
        {
            AZ_Warning("GOAT", planned == nullptr,
                "Lua backend '%s' returned an empty plan for goal '%s', which counts as refusing the intent",
                m_name.GetCStr(), intent.m_goal.GetCStr());
            return false;
        }

        outPlan = *planned;

        AZ_Assert(!outPlan.IsEmpty(), "A backend that reports success must have produced at least one step");
        return true;
    }
} // namespace GOAT
