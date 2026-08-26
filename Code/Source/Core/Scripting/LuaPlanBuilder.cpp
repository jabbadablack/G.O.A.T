#include <Core/Scripting/LuaPlanBuilder.h>

#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>

namespace GOAT
{
    void LuaPlanBuilder::Configure(const ActionStateRegistry* actions, const IBlackboardSystem* blackboard)
    {
        m_actions = actions;
        m_blackboard = blackboard;
    }

    void LuaPlanBuilder::BeginPlan()
    {
        m_plan.m_steps.clear();
        m_failed = false;
    }

    void LuaPlanBuilder::AddStep(AZStd::string verb)
    {
        if (m_failed)
        {
            return;
        }

        if (m_plan.m_steps.size() >= MaxPlanLength)
        {
            AZ_Warning("GOAT", false, "A Lua backend returned more than %zu steps", MaxPlanLength);
            m_failed = true;
            return;
        }

        const AZ::Name verbName(verb);
        const ActionStateId id = m_actions != nullptr ? m_actions->FindId(verbName) : CoreActions::Invalid;
        if (id == CoreActions::Invalid)
        {
            AZ_Warning("GOAT", false, "A Lua backend asked for unregistered verb '%s'", verbName.GetCStr());
            m_failed = true;
            return;
        }

        ActionRequest request;
        request.m_action = id;
        m_plan.m_steps.push_back(AZStd::move(request));
    }

    void LuaPlanBuilder::SetTag(AZStd::string tag)
    {
        if (!m_failed && !m_plan.m_steps.empty())
        {
            m_plan.m_steps.back().m_tag = AZ::Name(tag);
        }
    }

    void LuaPlanBuilder::SetDuration(double seconds)
    {
        if (!m_failed && !m_plan.m_steps.empty())
        {
            m_plan.m_steps.back().m_duration = static_cast<float>(seconds);
        }
    }

    void LuaPlanBuilder::SetTolerance(double tolerance)
    {
        if (!m_failed && !m_plan.m_steps.empty())
        {
            m_plan.m_steps.back().m_tolerance = static_cast<float>(tolerance);
        }
    }

    void LuaPlanBuilder::SetTargetKey(AZStd::string blackboardName)
    {
        if (m_failed || m_plan.m_steps.empty() || m_blackboard == nullptr)
        {
            return;
        }

        const BlackboardKey key = m_blackboard->FindKey(AZ::Name(blackboardName));
        if (!key.IsValid())
        {
            AZ_Warning("GOAT", false, "A Lua backend referred to undeclared variable '%s'", blackboardName.c_str());
            m_failed = true;
            return;
        }
        m_plan.m_steps.back().m_targetKey = key;
    }

    bool LuaPlanBuilder::EndPlan()
    {
        return !m_failed && !m_plan.IsEmpty();
    }

    void LuaPlanBuilder::Reflect(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->Class<LuaPlanBuilder>("GoatPlanBuilder")
            ->Attribute(AZ::Script::Attributes::Category, "GOAT")
            ->Method("BeginPlan", &LuaPlanBuilder::BeginPlan)
            ->Method("AddStep", &LuaPlanBuilder::AddStep)
            ->Method("SetTag", &LuaPlanBuilder::SetTag)
            ->Method("SetDuration", &LuaPlanBuilder::SetDuration)
            ->Method("SetTolerance", &LuaPlanBuilder::SetTolerance)
            ->Method("SetTargetKey", &LuaPlanBuilder::SetTargetKey)
            ->Method("EndPlan", &LuaPlanBuilder::EndPlan);
    }
} // namespace GOAT
