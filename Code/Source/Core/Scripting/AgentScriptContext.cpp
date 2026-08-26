#include <Core/Scripting/AgentScriptContext.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>

namespace GOAT
{
    void AgentScriptContext::Bind(AgentId agent, AZ::EntityId entity, IBlackboardSystem* blackboard)
    {
        m_agent = agent;
        m_entity = entity;
        m_blackboard = blackboard;
    }

    void AgentScriptContext::Unbind()
    {
        m_agent = AgentId{};
        m_entity = AZ::EntityId{};
        m_blackboard = nullptr;
    }

    bool AgentScriptContext::Has(const AZStd::string& name) const
    {
        return m_blackboard != nullptr && m_blackboard->FindKey(AZ::Name(name)).IsValid();
    }

    bool AgentScriptContext::GetBool(const AZStd::string& name) const
    {
        if (m_blackboard == nullptr)
        {
            return false;
        }
        const bool* value = m_blackboard->Find<bool>(m_blackboard->FindKey(AZ::Name(name)), m_agent);
        return value != nullptr && *value;
    }

    void AgentScriptContext::SetBool(const AZStd::string& name, bool value)
    {
        if (m_blackboard != nullptr)
        {
            m_blackboard->Set<bool>(m_blackboard->FindKey(AZ::Name(name)), value, m_agent);
        }
    }

    double AgentScriptContext::GetNumber(const AZStd::string& name) const
    {
        if (m_blackboard == nullptr)
        {
            return 0.0;
        }

        const BlackboardKey key = m_blackboard->FindKey(AZ::Name(name));
        if (key.GetType() == BlackboardType::Int)
        {
            const AZ::s64* value = m_blackboard->Find<AZ::s64>(key, m_agent);
            return value != nullptr ? static_cast<double>(*value) : 0.0;
        }

        const float* value = m_blackboard->Find<float>(key, m_agent);
        return value != nullptr ? static_cast<double>(*value) : 0.0;
    }

    void AgentScriptContext::SetNumber(const AZStd::string& name, double value)
    {
        if (m_blackboard == nullptr)
        {
            return;
        }

        // Lua has one number type, so route it to whichever numeric slot was declared.
        const BlackboardKey key = m_blackboard->FindKey(AZ::Name(name));
        if (key.GetType() == BlackboardType::Int)
        {
            m_blackboard->Set<AZ::s64>(key, static_cast<AZ::s64>(value), m_agent);
        }
        else
        {
            m_blackboard->Set<float>(key, static_cast<float>(value), m_agent);
        }
    }

    AZ::Vector3 AgentScriptContext::GetVector3(const AZStd::string& name) const
    {
        if (m_blackboard == nullptr)
        {
            return AZ::Vector3::CreateZero();
        }
        const AZ::Vector3* value = m_blackboard->Find<AZ::Vector3>(m_blackboard->FindKey(AZ::Name(name)), m_agent);
        return value != nullptr ? *value : AZ::Vector3::CreateZero();
    }

    void AgentScriptContext::SetVector3(const AZStd::string& name, const AZ::Vector3& value)
    {
        if (m_blackboard != nullptr)
        {
            m_blackboard->Set<AZ::Vector3>(m_blackboard->FindKey(AZ::Name(name)), value, m_agent);
        }
    }

    AZ::EntityId AgentScriptContext::GetEntity(const AZStd::string& name) const
    {
        if (m_blackboard == nullptr)
        {
            return AZ::EntityId{};
        }
        const AZ::EntityId* value = m_blackboard->Find<AZ::EntityId>(m_blackboard->FindKey(AZ::Name(name)), m_agent);
        return value != nullptr ? *value : AZ::EntityId{};
    }

    void AgentScriptContext::SetEntity(const AZStd::string& name, AZ::EntityId value)
    {
        if (m_blackboard != nullptr)
        {
            m_blackboard->Set<AZ::EntityId>(m_blackboard->FindKey(AZ::Name(name)), value, m_agent);
        }
    }

    AZStd::string AgentScriptContext::GetName(const AZStd::string& name) const
    {
        if (m_blackboard == nullptr)
        {
            return {};
        }
        const AZ::Name* value = m_blackboard->Find<AZ::Name>(m_blackboard->FindKey(AZ::Name(name)), m_agent);
        return value != nullptr ? AZStd::string(value->GetStringView()) : AZStd::string{};
    }

    void AgentScriptContext::SetName(const AZStd::string& name, const AZStd::string& value)
    {
        if (m_blackboard != nullptr)
        {
            m_blackboard->Set<AZ::Name>(m_blackboard->FindKey(AZ::Name(name)), AZ::Name(value), m_agent);
        }
    }

    void AgentScriptContext::Reflect(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        behaviorContext->Class<AgentScriptContext>("GoatAgent")
            ->Attribute(AZ::Script::Attributes::Category, "GOAT")
            ->Method("GetSelf", &AgentScriptContext::GetSelf)
            ->Method("Has", &AgentScriptContext::Has)
            ->Method("GetBool", &AgentScriptContext::GetBool)
            ->Method("SetBool", &AgentScriptContext::SetBool)
            ->Method("GetNumber", &AgentScriptContext::GetNumber)
            ->Method("SetNumber", &AgentScriptContext::SetNumber)
            ->Method("GetInt", &AgentScriptContext::GetNumber)
            ->Method("SetInt", &AgentScriptContext::SetNumber)
            ->Method("GetFloat", &AgentScriptContext::GetNumber)
            ->Method("SetFloat", &AgentScriptContext::SetNumber)
            ->Method("GetVector3", &AgentScriptContext::GetVector3)
            ->Method("SetVector3", &AgentScriptContext::SetVector3)
            ->Method("GetEntity", &AgentScriptContext::GetEntity)
            ->Method("SetEntity", &AgentScriptContext::SetEntity)
            ->Method("GetName", &AgentScriptContext::GetName)
            ->Method("SetName", &AgentScriptContext::SetName);
    }
} // namespace GOAT
