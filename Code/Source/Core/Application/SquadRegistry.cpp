#include <Core/Application/SquadRegistry.h>

namespace GOAT
{
    void SquadRegistry::Join(AgentId agent, const AZ::Name& squad, const BlackboardLayout& layout)
    {
        if (agent.IsNull() || squad.IsEmpty())
        {
            return;
        }

        if (Find(agent) == squad)
        {
            return;
        }

        Leave(agent);

        Squad& entry = m_squads[squad];
        if (entry.m_memberCount == 0)
        {
            entry.m_storage.Reset(layout);
        }
        ++entry.m_memberCount;

        m_squadByAgent[agent] = squad;
    }

    void SquadRegistry::Leave(AgentId agent)
    {
        const auto membership = m_squadByAgent.find(agent);
        if (membership == m_squadByAgent.end())
        {
            return;
        }

        const auto squad = m_squads.find(membership->second);
        if (squad != m_squads.end())
        {
            --squad->second.m_memberCount;
            if (squad->second.m_memberCount == 0)
            {
                m_squads.erase(squad);
            }
        }

        m_squadByAgent.erase(membership);
    }

    AZ::Name SquadRegistry::Find(AgentId agent) const
    {
        const auto membership = m_squadByAgent.find(agent);
        return membership != m_squadByAgent.end() ? membership->second : AZ::Name{};
    }

    BlackboardStorage* SquadRegistry::FindStorage(AgentId agent)
    {
        return const_cast<BlackboardStorage*>(static_cast<const SquadRegistry*>(this)->FindStorage(agent));
    }

    const BlackboardStorage* SquadRegistry::FindStorage(AgentId agent) const
    {
        const auto membership = m_squadByAgent.find(agent);
        if (membership == m_squadByAgent.end())
        {
            return nullptr;
        }

        const auto squad = m_squads.find(membership->second);
        return squad != m_squads.end() ? &squad->second.m_storage : nullptr;
    }

    void SquadRegistry::EnsureCapacity(const BlackboardLayout& layout)
    {
        for (auto& [name, squad] : m_squads)
        {
            squad.m_storage.EnsureCapacity(layout);
        }
    }

    void SquadRegistry::Clear()
    {
        m_squads.clear();
        m_squadByAgent.clear();
    }
} // namespace GOAT
