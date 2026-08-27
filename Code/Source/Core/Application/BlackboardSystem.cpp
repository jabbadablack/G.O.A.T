#include <Core/Application/BlackboardSystem.h>

#include <AzCore/Console/ILogger.h>

namespace GOAT
{
    BlackboardSystem::BlackboardSystem()
    {
        AZ_Assert(BlackboardSystemInterface::Get() == nullptr, "Only one blackboard system may exist at a time");
        BlackboardSystemInterface::Register(this);
    }

    BlackboardSystem::~BlackboardSystem()
    {
        AZ_Assert(BlackboardSystemInterface::Get() == this, "The blackboard system was replaced before it was destroyed");
        BlackboardSystemInterface::Unregister(this);
    }

    AZ::Outcome<BlackboardKey, AZStd::string> BlackboardSystem::Declare(
        const AZ::Name& name, BlackboardScope scope, BlackboardType type, AZStd::any defaultValue)
    {
        AZ_Assert(!name.IsEmpty(), "A blackboard variable must be declared under a name");
        AZ_Assert(scope < BlackboardScope::Count, "Blackboard scope is out of range");
        AZ_Assert(type < BlackboardType::Count, "Blackboard type is out of range");

        auto declared = m_schema.Declare(name, scope, type, AZStd::move(defaultValue));
        if (!declared.IsSuccess())
        {
            return declared;
        }

        AZ_Assert(declared.GetValue().IsValid(), "A successful declaration must yield a valid key");
        AZ_Assert(m_schema.Find(name) == declared.GetValue(), "A declared name must resolve back to the key it was given");

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
        AZ_Assert(!agent.IsNull(), "Agent scoped storage cannot be created for a null agent");
        if (agent.IsNull())
        {
            AZ_Error("GOAT", false, "Refusing to create agent blackboard storage for a null agent");
            return;
        }

        m_agents[agent].Reset(m_schema.GetLayout(BlackboardScope::Agent));

        AZ_Assert(m_agents.find(agent) != m_agents.end(), "Creating agent storage must leave it findable");
    }

    void BlackboardSystem::DestroyAgentBlackboard(AgentId agent)
    {
        // Leaving first is what refcounts the squad down, so the order here is load bearing.
        m_squads.Leave(agent);
        m_agents.erase(agent);
        AZ_Assert(m_squads.Find(agent).IsEmpty(), "A destroyed agent must not still belong to a squad");
    }

    void BlackboardSystem::JoinSquad(AgentId agent, const AZ::Name& squad)
    {
        AZ_Assert(!agent.IsNull(), "A null agent cannot join a squad");
        AZ_Assert(!squad.IsEmpty(), "A squad must be joined by name");
        if (agent.IsNull() || squad.IsEmpty())
        {
            AZ_Error("GOAT", false, "Refusing to join squad '%s': the agent or the squad name is missing", squad.GetCStr());
            return;
        }

        m_squads.Join(agent, squad, m_schema.GetLayout(BlackboardScope::Squad));

        AZ_Assert(m_squads.Find(agent) == squad, "Joining a squad must leave the agent in that squad");
    }

    void BlackboardSystem::LeaveSquad(AgentId agent)
    {
        m_squads.Leave(agent);
        AZ_Assert(m_squads.Find(agent).IsEmpty(), "Leaving a squad must leave the agent in none");
    }

    AZ::Name BlackboardSystem::GetSquad(AgentId agent) const
    {
        return m_squads.Find(agent);
    }

    BlackboardStorage* BlackboardSystem::FindSquadStorage(const AZ::Name& squad)
    {
        AZ_Assert(!squad.IsEmpty(), "Squad storage is only looked up by a name");
        return m_squads.FindStorage(squad);
    }

    AZStd::vector<AZ::Name> BlackboardSystem::GetSquadNames() const
    {
        return m_squads.GetNames();
    }

    BlackboardStorage* BlackboardSystem::FindStorage(BlackboardScope scope, AgentId agent)
    {
        return const_cast<BlackboardStorage*>(static_cast<const BlackboardSystem*>(this)->FindStorage(scope, agent));
    }

    const BlackboardStorage* BlackboardSystem::FindStorage(BlackboardScope scope, AgentId agent) const
    {
        AZ_Assert(scope < BlackboardScope::Count, "Blackboard scope is out of range");
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
} // namespace GOAT
