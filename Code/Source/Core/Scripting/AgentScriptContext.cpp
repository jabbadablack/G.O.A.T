#include <Core/Scripting/AgentScriptContext.h>

#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/Name.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Script/ScriptContextAttributes.h>

namespace GOAT
{
    BlackboardKey AgentScriptContext::Resolve(const AZStd::string& name, const char* access) const
    {
        AZ_Assert(access != nullptr, "A resolve always names what the script was doing");
        AZ_Assert(m_blackboard != nullptr, "A script only reaches the blackboard while this context is bound");

        if (m_blackboard == nullptr)
        {
            AZ_Error("GOAT", false, "A script tried to %s '%s' outside a behaviour, where no agent is bound",
                access, name.c_str());
            return BlackboardKey{};
        }

        const BlackboardKey key = m_blackboard->FindKey(AZ::Name(name));
        AZ_Warning("GOAT", key.IsValid(),
            "A script tried to %s '%s', which no .bbx file declares; check the spelling", access, name.c_str());
        return key;
    }

    void AgentScriptContext::Bind(AgentId agent, AZ::EntityId entity, IBlackboardSystem* blackboard)
    {
        AZ_Assert(blackboard != nullptr, "A script context is only bound to a running blackboard system");
        m_agent = agent;
        m_entity = entity;
        m_blackboard = blackboard;

        AZ_Assert(m_blackboard == blackboard, "Binding must leave this context pointing at that blackboard");
    }

    void AgentScriptContext::Unbind()
    {
        m_agent = AgentId{};
        m_entity = AZ::EntityId{};
        m_blackboard = nullptr;

        AZ_Assert(m_agent.IsNull() && m_blackboard == nullptr, "Unbinding must leave nothing reachable from Lua");
    }

    bool AgentScriptContext::Has(const AZStd::string& name) const
    {
        return m_blackboard != nullptr && m_blackboard->FindKey(AZ::Name(name)).IsValid();
    }

    bool AgentScriptContext::GetBool(const AZStd::string& name) const
    {
        const BlackboardKey key = Resolve(name, "read boolean");
        if (!key.IsValid())
        {
            return false;
        }

        const bool* value = m_blackboard->Find<bool>(key, m_agent);
        AZ_Warning("GOAT", value != nullptr, "'%s' is not a boolean for agent %u, so it reads as false",
            name.c_str(), m_agent.GetIndex());
        return value != nullptr && *value;
    }

    void AgentScriptContext::SetBool(const AZStd::string& name, bool value)
    {
        const BlackboardKey key = Resolve(name, "write boolean");
        if (!key.IsValid())
        {
            return;
        }

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool written = m_blackboard->Set<bool>(key, value, m_agent);
        AZ_Warning("GOAT", written, "Writing boolean '%s' for agent %u failed; it is declared as another type",
            name.c_str(), m_agent.GetIndex());
    }

    double AgentScriptContext::GetNumber(const AZStd::string& name) const
    {
        const BlackboardKey key = Resolve(name, "read number");
        if (!key.IsValid())
        {
            return 0.0;
        }

        if (key.GetType() == BlackboardType::Int)
        {
            const AZ::s64* value = m_blackboard->Find<AZ::s64>(key, m_agent);
            return value != nullptr ? static_cast<double>(*value) : 0.0;
        }

        AZ_Warning("GOAT", key.GetType() == BlackboardType::Float,
            "'%s' is not a number, so it reads as zero", name.c_str());

        const float* value = m_blackboard->Find<float>(key, m_agent);
        return value != nullptr ? static_cast<double>(*value) : 0.0;
    }

    void AgentScriptContext::SetNumber(const AZStd::string& name, double value)
    {
        // Lua has one number type, so route it to whichever numeric slot was declared.
        const BlackboardKey key = Resolve(name, "write number");
        if (!key.IsValid())
        {
            return;
        }

        const bool written = key.GetType() == BlackboardType::Int
            ? m_blackboard->Set<AZ::s64>(key, static_cast<AZ::s64>(value), m_agent)
            : m_blackboard->Set<float>(key, static_cast<float>(value), m_agent);

        AZ_Warning("GOAT", written, "Writing number '%s' for agent %u failed; it is declared as a non numeric type",
            name.c_str(), m_agent.GetIndex());
    }

    AZ::Vector3 AgentScriptContext::GetVector3(const AZStd::string& name) const
    {
        const BlackboardKey key = Resolve(name, "read vector");
        if (!key.IsValid())
        {
            return AZ::Vector3::CreateZero();
        }

        const AZ::Vector3* value = m_blackboard->Find<AZ::Vector3>(key, m_agent);
        AZ_Warning("GOAT", value != nullptr, "'%s' is not a vector for agent %u, so it reads as zero",
            name.c_str(), m_agent.GetIndex());
        return value != nullptr ? *value : AZ::Vector3::CreateZero();
    }

    void AgentScriptContext::SetVector3(const AZStd::string& name, const AZ::Vector3& value)
    {
        const BlackboardKey key = Resolve(name, "write vector");
        if (!key.IsValid())
        {
            return;
        }

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool written = m_blackboard->Set<AZ::Vector3>(key, value, m_agent);
        AZ_Warning("GOAT", written, "Writing vector '%s' for agent %u failed; it is declared as another type",
            name.c_str(), m_agent.GetIndex());
    }

    AZ::EntityId AgentScriptContext::GetEntity(const AZStd::string& name) const
    {
        const BlackboardKey key = Resolve(name, "read entity");
        if (!key.IsValid())
        {
            return AZ::EntityId{};
        }

        const AZ::EntityId* value = m_blackboard->Find<AZ::EntityId>(key, m_agent);
        AZ_Warning("GOAT", value != nullptr, "'%s' is not an entity for agent %u, so it reads as invalid",
            name.c_str(), m_agent.GetIndex());
        return value != nullptr ? *value : AZ::EntityId{};
    }

    void AgentScriptContext::SetEntity(const AZStd::string& name, AZ::EntityId value)
    {
        const BlackboardKey key = Resolve(name, "write entity");
        if (!key.IsValid())
        {
            return;
        }

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool written = m_blackboard->Set<AZ::EntityId>(key, value, m_agent);
        AZ_Warning("GOAT", written, "Writing entity '%s' for agent %u failed; it is declared as another type",
            name.c_str(), m_agent.GetIndex());
    }

    AZStd::string AgentScriptContext::GetName(const AZStd::string& name) const
    {
        const BlackboardKey key = Resolve(name, "read name");
        if (!key.IsValid())
        {
            return {};
        }

        const AZ::Name* value = m_blackboard->Find<AZ::Name>(key, m_agent);
        AZ_Warning("GOAT", value != nullptr, "'%s' is not a name for agent %u, so it reads as empty",
            name.c_str(), m_agent.GetIndex());
        return value != nullptr ? AZStd::string(value->GetStringView()) : AZStd::string{};
    }

    void AgentScriptContext::SetName(const AZStd::string& name, const AZStd::string& value)
    {
        const BlackboardKey key = Resolve(name, "write name");
        if (!key.IsValid())
        {
            return;
        }

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool written = m_blackboard->Set<AZ::Name>(key, AZ::Name(value), m_agent);
        AZ_Warning("GOAT", written, "Writing name '%s' for agent %u failed; it is declared as another type",
            name.c_str(), m_agent.GetIndex());
    }

    bool AgentScriptContext::SetTree(const AZStd::string& treeName)
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        AZ_Warning("GOAT", agents != nullptr, "A script asked for tree '%s' with no agent system running",
            treeName.c_str());
        return agents != nullptr && agents->SetAgentTree(m_agent, AZ::Name(treeName));
    }

    bool AgentScriptContext::PushTree(const AZStd::string& treeName)
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        AZ_Warning("GOAT", agents != nullptr, "A script asked to push tree '%s' with no agent system running",
            treeName.c_str());
        return agents != nullptr && agents->PushAgentTree(m_agent, AZ::Name(treeName));
    }

    bool AgentScriptContext::PopTree()
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        return agents != nullptr && agents->PopAgentTree(m_agent);
    }

    AZStd::string AgentScriptContext::GetTree() const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            return {};
        }
        return AZStd::string(agents->GetAgentTree(m_agent).GetStringView());
    }

    int AgentScriptContext::CountInReach() const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        return agents != nullptr ? static_cast<int>(agents->GetReachSize(m_agent)) : 0;
    }

    int AgentScriptContext::CountRunning(const AZStd::string& treeName) const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            return 0;
        }

        const AZ::Name wanted(treeName);
        const size_t reach = agents->GetReachSize(m_agent);

        int running = 0;
        for (size_t i = 0; i < reach; ++i)
        {
            if (agents->GetAgentTree(agents->GetInReach(m_agent, i)) == wanted)
            {
                ++running;
            }
        }
        return running;
    }

    AZ::EntityId AgentScriptContext::GetInReach(int index) const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        AZ_Warning("GOAT", index >= 1, "A reach position is one based, to match Lua");
        if (agents == nullptr || index < 1)
        {
            return AZ::EntityId{};
        }

        const AgentId found = agents->GetInReach(m_agent, static_cast<size_t>(index - 1));
        return found.IsNull() ? AZ::EntityId{} : agents->GetAgentEntity(found);
    }

    AZStd::string AgentScriptContext::GetTreeOf(AZ::EntityId entity) const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            return {};
        }
        return AZStd::string(agents->GetAgentTree(agents->FindAgent(entity)).GetStringView());
    }

    AZStd::string AgentScriptContext::GetSquadOf(AZ::EntityId entity) const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            return {};
        }
        return AZStd::string(agents->GetAgentSquad(agents->FindAgent(entity)).GetStringView());
    }

    int AgentScriptContext::GetBandOf(AZ::EntityId entity) const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            return -1;
        }

        const AgentId found = agents->FindAgent(entity);
        return found.IsNull() ? -1 : static_cast<int>(agents->GetAgentBand(found));
    }

    double AgentScriptContext::GetNumberOf(AZ::EntityId entity, const AZStd::string& name) const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        const AgentId found = agents != nullptr ? agents->FindAgent(entity) : AgentId{};
        if (found.IsNull() || m_blackboard == nullptr)
        {
            return 0.0;
        }

        const BlackboardKey key = m_blackboard->FindKey(AZ::Name(name));
        if (!key.IsValid())
        {
            return 0.0;
        }

        if (key.GetType() == BlackboardType::Int)
        {
            const AZ::s64* value = m_blackboard->Find<AZ::s64>(key, found);
            return value != nullptr ? static_cast<double>(*value) : 0.0;
        }

        const float* value = m_blackboard->Find<float>(key, found);
        return value != nullptr ? static_cast<double>(*value) : 0.0;
    }

    bool AgentScriptContext::GetBoolOf(AZ::EntityId entity, const AZStd::string& name) const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        const AgentId found = agents != nullptr ? agents->FindAgent(entity) : AgentId{};
        if (found.IsNull() || m_blackboard == nullptr)
        {
            return false;
        }

        const BlackboardKey key = m_blackboard->FindKey(AZ::Name(name));
        const bool* value = key.IsValid() ? m_blackboard->Find<bool>(key, found) : nullptr;
        return value != nullptr && *value;
    }

    void AgentScriptContext::Reflect(AZ::ReflectContext* context)
    {
        auto* behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context);
        if (behaviorContext == nullptr)
        {
            return;
        }

        AZ_Assert(context != nullptr, "Reflection always runs against a context");

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
            ->Method("SetName", &AgentScriptContext::SetName)
            ->Method("SetTree", &AgentScriptContext::SetTree)
            ->Method("PushTree", &AgentScriptContext::PushTree)
            ->Method("PopTree", &AgentScriptContext::PopTree)
            ->Method("GetTree", &AgentScriptContext::GetTree)
            ->Method("CountInReach", &AgentScriptContext::CountInReach)
            ->Method("CountRunning", &AgentScriptContext::CountRunning)
            ->Method("GetInReach", &AgentScriptContext::GetInReach)
            ->Method("GetTreeOf", &AgentScriptContext::GetTreeOf)
            ->Method("GetSquadOf", &AgentScriptContext::GetSquadOf)
            ->Method("GetBandOf", &AgentScriptContext::GetBandOf)
            ->Method("GetNumberOf", &AgentScriptContext::GetNumberOf)
            ->Method("GetBoolOf", &AgentScriptContext::GetBoolOf);
    }
} // namespace GOAT
