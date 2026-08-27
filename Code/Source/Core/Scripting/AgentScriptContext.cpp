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

    double AgentScriptContext::Key(const AZStd::string& name) const
    {
        const BlackboardKey key = Resolve(name, "look up");

        // Offset by one so that zero is never a handle. A packed key of zero is a real slot --
        // the first global bool -- while zero is also what an absent or nil Lua argument arrives
        // as, and those two must not be the same thing. Without this a script that lost its
        // handle reads someone else's variable and reports nothing.
        return key.IsValid() ? static_cast<double>(key.GetPacked()) + 1.0 : 0.0;
    }

    BlackboardKey AgentScriptContext::FromLua(double key, const char* access) const
    {
        if (m_blackboard == nullptr)
        {
            AZ_Error("GOAT", false, "A script tried to %s outside a behaviour, where no agent is bound", access);
            return BlackboardKey{};
        }

        const BlackboardKey resolved =
            key >= 1.0 ? BlackboardKey::FromPacked(static_cast<AZ::u32>(key) - 1u) : BlackboardKey{};
        AZ_Warning("GOAT", resolved.IsValid(),
            "A script tried to %s through a handle ctx:Key never gave it; look the variable up once and keep that",
            access);
        return resolved;
    }

    void AgentScriptContext::Bind(AgentId agent, AZ::EntityId entity, IBlackboardSystem* blackboard)
    {
        AZ_Assert(blackboard != nullptr, "A script context is only bound to a running blackboard system");
        m_agent = agent;
        m_entity = entity;
        m_blackboard = blackboard;
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

    bool AgentScriptContext::GetBool(double key) const
    {
        const BlackboardKey slot = FromLua(key, "read boolean");
        if (!slot.IsValid())
        {
            return false;
        }

        const bool* value = m_blackboard->Find<bool>(slot, m_agent);
        AZ_Warning("GOAT", value != nullptr, "the slot is not a boolean for agent %u, so it reads as false",
            m_agent.GetIndex());
        return value != nullptr && *value;
    }

    void AgentScriptContext::SetBool(double key, bool value)
    {
        const BlackboardKey slot = FromLua(key, "write boolean");
        if (!slot.IsValid())
        {
            return;
        }

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool written = m_blackboard->Set<bool>(slot, value, m_agent);
        AZ_Warning("GOAT", written, "Writing a boolean for agent %u failed; it is declared as another type",
            m_agent.GetIndex());
    }

    double AgentScriptContext::GetNumber(double key) const
    {
        const BlackboardKey slot = FromLua(key, "read number");
        if (!slot.IsValid())
        {
            return 0.0;
        }

        if (slot.GetType() == BlackboardType::Int)
        {
            const AZ::s64* value = m_blackboard->Find<AZ::s64>(slot, m_agent);
            return value != nullptr ? static_cast<double>(*value) : 0.0;
        }

        AZ_Warning("GOAT", slot.GetType() == BlackboardType::Float,
            "the slot is not a number, so it reads as zero");

        const float* value = m_blackboard->Find<float>(slot, m_agent);
        return value != nullptr ? static_cast<double>(*value) : 0.0;
    }

    void AgentScriptContext::SetNumber(double key, double value)
    {
        // Lua has one number type, so route it to whichever numeric slot was declared.
        const BlackboardKey slot = FromLua(key, "write number");
        if (!slot.IsValid())
        {
            return;
        }

        const bool written = slot.GetType() == BlackboardType::Int
            ? m_blackboard->Set<AZ::s64>(slot, static_cast<AZ::s64>(value), m_agent)
            : m_blackboard->Set<float>(slot, static_cast<float>(value), m_agent);

        AZ_Warning("GOAT", written, "Writing a number for agent %u failed; it is declared as a non numeric type",
            m_agent.GetIndex());
    }

    AZ::Vector3 AgentScriptContext::GetVector3(double key) const
    {
        const BlackboardKey slot = FromLua(key, "read vector");
        if (!slot.IsValid())
        {
            return AZ::Vector3::CreateZero();
        }

        const AZ::Vector3* value = m_blackboard->Find<AZ::Vector3>(slot, m_agent);
        AZ_Warning("GOAT", value != nullptr, "the slot is not a vector for agent %u, so it reads as zero",
            m_agent.GetIndex());
        return value != nullptr ? *value : AZ::Vector3::CreateZero();
    }

    void AgentScriptContext::SetVector3(double key, const AZ::Vector3& value)
    {
        const BlackboardKey slot = FromLua(key, "write vector");
        if (!slot.IsValid())
        {
            return;
        }

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool written = m_blackboard->Set<AZ::Vector3>(slot, value, m_agent);
        AZ_Warning("GOAT", written, "Writing a vector for agent %u failed; it is declared as another type",
            m_agent.GetIndex());
    }

    AZ::EntityId AgentScriptContext::GetEntity(double key) const
    {
        const BlackboardKey slot = FromLua(key, "read entity");
        if (!slot.IsValid())
        {
            return AZ::EntityId{};
        }

        const AZ::EntityId* value = m_blackboard->Find<AZ::EntityId>(slot, m_agent);
        AZ_Warning("GOAT", value != nullptr, "the slot is not an entity for agent %u, so it reads as invalid",
            m_agent.GetIndex());
        return value != nullptr ? *value : AZ::EntityId{};
    }

    void AgentScriptContext::SetEntity(double key, AZ::EntityId value)
    {
        const BlackboardKey slot = FromLua(key, "write entity");
        if (!slot.IsValid())
        {
            return;
        }

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool written = m_blackboard->Set<AZ::EntityId>(slot, value, m_agent);
        AZ_Warning("GOAT", written, "Writing an entity for agent %u failed; it is declared as another type",
            m_agent.GetIndex());
    }

    AZStd::string AgentScriptContext::GetName(double key) const
    {
        const BlackboardKey slot = FromLua(key, "read name");
        if (!slot.IsValid())
        {
            return {};
        }

        const AZ::Name* value = m_blackboard->Find<AZ::Name>(slot, m_agent);
        AZ_Warning("GOAT", value != nullptr, "the slot is not a name for agent %u, so it reads as empty",
            m_agent.GetIndex());
        return value != nullptr ? AZStd::string(value->GetStringView()) : AZStd::string{};
    }

    void AgentScriptContext::SetName(double key, const AZStd::string& value)
    {
        const BlackboardKey slot = FromLua(key, "write name");
        if (!slot.IsValid())
        {
            return;
        }

        // Hoisted out of the warning on purpose: a trace macro's expression is not compiled in release.
        const bool written = m_blackboard->Set<AZ::Name>(slot, AZ::Name(value), m_agent);
        AZ_Warning("GOAT", written, "Writing a name for agent %u failed; it is declared as another type",
            m_agent.GetIndex());
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

    double AgentScriptContext::GetNumberOf(AZ::EntityId entity, double key) const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        const AgentId found = agents != nullptr ? agents->FindAgent(entity) : AgentId{};
        if (found.IsNull() || m_blackboard == nullptr)
        {
            return 0.0;
        }

        // The same handle answers for every agent: a variable's slot is fixed by the schema,
        // not by whose blackboard is being read.
        const BlackboardKey slot = FromLua(key, "read another agent's number");
        if (!slot.IsValid())
        {
            return 0.0;
        }

        if (slot.GetType() == BlackboardType::Int)
        {
            const AZ::s64* value = m_blackboard->Find<AZ::s64>(slot, found);
            return value != nullptr ? static_cast<double>(*value) : 0.0;
        }

        const float* value = m_blackboard->Find<float>(slot, found);
        return value != nullptr ? static_cast<double>(*value) : 0.0;
    }

    bool AgentScriptContext::GetBoolOf(AZ::EntityId entity, double key) const
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        const AgentId found = agents != nullptr ? agents->FindAgent(entity) : AgentId{};
        if (found.IsNull() || m_blackboard == nullptr)
        {
            return false;
        }

        const BlackboardKey slot = FromLua(key, "read another agent's boolean");
        const bool* value = slot.IsValid() ? m_blackboard->Find<bool>(slot, found) : nullptr;
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
            ->Method("Key", &AgentScriptContext::Key)
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
