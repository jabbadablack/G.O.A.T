#include <Core/Application/BlackboardSystem.h>

namespace GOAT
{
    BlackboardSystem::BlackboardSystem()
    {
        BlackboardSystemInterface::Register(this);
    }

    BlackboardSystem::~BlackboardSystem()
    {
        BlackboardSystemInterface::Unregister(this);
    }

    AZ::Outcome<BlackboardKey, AZStd::string> BlackboardSystem::Declare(
        const AZ::Name& name, BlackboardScope scope, BlackboardType type, AZStd::any defaultValue)
    {
        auto declared = m_schema.Declare(name, scope, type, AZStd::move(defaultValue));
        if (!declared.IsSuccess())
        {
            return declared;
        }

        // Grow whatever already exists so a late declaration does not disturb live agents.
        switch (scope)
        {
        case BlackboardScope::Global:
            m_global.EnsureCapacity(m_schema.GetLayout(BlackboardScope::Global));
            break;
        case BlackboardScope::Agent:
            for (auto& [agent, storage] : m_agents)
            {
                storage.EnsureCapacity(m_schema.GetLayout(BlackboardScope::Agent));
            }
            break;
        case BlackboardScope::Squad:
            m_squads.EnsureCapacity(m_schema.GetLayout(BlackboardScope::Squad));
            break;
        default:
            break;
        }

        return declared;
    }

    BlackboardKey BlackboardSystem::FindKey(const AZ::Name& name) const
    {
        return m_schema.Find(name);
    }

    void BlackboardSystem::CreateAgentBlackboard(AgentId agent)
    {
        if (agent.IsNull())
        {
            return;
        }

        m_agents[agent].Reset(m_schema.GetLayout(BlackboardScope::Agent));
    }

    void BlackboardSystem::DestroyAgentBlackboard(AgentId agent)
    {
        m_squads.Leave(agent);
        m_agents.erase(agent);
    }

    void BlackboardSystem::JoinSquad(AgentId agent, const AZ::Name& squad)
    {
        m_squads.Join(agent, squad, m_schema.GetLayout(BlackboardScope::Squad));
    }

    void BlackboardSystem::LeaveSquad(AgentId agent)
    {
        m_squads.Leave(agent);
    }

    AZ::Name BlackboardSystem::GetSquad(AgentId agent) const
    {
        return m_squads.Find(agent);
    }

    BlackboardStorage* BlackboardSystem::FindStorage(BlackboardScope scope, AgentId agent)
    {
        return const_cast<BlackboardStorage*>(static_cast<const BlackboardSystem*>(this)->FindStorage(scope, agent));
    }

    const BlackboardStorage* BlackboardSystem::FindStorage(BlackboardScope scope, AgentId agent) const
    {
        switch (scope)
        {
        case BlackboardScope::Global:
            return &m_global;
        case BlackboardScope::Agent:
        {
            const auto found = m_agents.find(agent);
            return found != m_agents.end() ? &found->second : nullptr;
        }
        case BlackboardScope::Squad:
            return m_squads.FindStorage(agent);
        default:
            return nullptr;
        }
    }

    void BlackboardSystem::Clear()
    {
        m_squads.Clear();
        m_agents.clear();
        m_schema.Clear();
        m_global.Reset(m_schema.GetLayout(BlackboardScope::Global));
    }
} // namespace GOAT
