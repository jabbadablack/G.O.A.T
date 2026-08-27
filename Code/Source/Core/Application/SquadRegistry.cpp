#include <Core/Application/SquadRegistry.h>

#include <AzCore/Console/ILogger.h>

namespace GOAT
{
    void SquadRegistry::Join(AgentId agent, const AZ::Name& squad, const BlackboardLayout& layout)
    {
        AZ_Assert(!agent.IsNull(), "A null agent cannot join a squad");
        AZ_Assert(!squad.IsEmpty(), "A squad is always joined by name");
        if (agent.IsNull() || squad.IsEmpty())
        {
            return;
        }

        if (Find(agent) == squad)
        {
            return;
        }

        // An agent belongs to at most one squad, so the old membership is refcounted down first.
        Leave(agent);
        AZ_Assert(Find(agent).IsEmpty(), "An agent must have left its old squad before joining another");

        Squad& entry = m_squads[squad];
        if (entry.m_memberCount == 0)
        {
            AZLOG_INFO("GOAT: squad '%s' created by its first member", squad.GetCStr());
            entry.m_storage.Reset(layout);
        }
        ++entry.m_memberCount;

        m_squadByAgent[agent] = squad;

        AZ_Assert(Find(agent) == squad, "Joining must record the squad the agent joined");
        AZ_Assert(entry.m_memberCount > 0, "A squad that has been joined has at least one member");
    }

    void SquadRegistry::Leave(AgentId agent)
    {
        const auto membership = m_squadByAgent.find(agent);
        if (membership == m_squadByAgent.end())
        {
            return;
        }

        const auto squad = m_squads.find(membership->second);
        AZ_Assert(squad != m_squads.end(), "An agent's recorded squad must exist while it is a member");
        if (squad != m_squads.end())
        {
            AZ_Assert(squad->second.m_memberCount > 0, "A squad with a member cannot have a zero member count");

            --squad->second.m_memberCount;
            if (squad->second.m_memberCount == 0)
            {
                AZLOG_INFO("GOAT: squad '%s' destroyed by its last member leaving", squad->first.GetCStr());
                m_squads.erase(squad);
            }
        }

        m_squadByAgent.erase(membership);

        AZ_Assert(Find(agent).IsEmpty(), "Leaving must leave the agent in no squad");
    }

    AZ::Name SquadRegistry::Find(AgentId agent) const
    {
        const auto membership = m_squadByAgent.find(agent);
        return membership != m_squadByAgent.end() ? membership->second : AZ::Name{};
    }

    void SquadRegistry::FindMembers(const AZ::Name& squad, AZStd::vector<AgentId>& out) const
    {
        AZ_Assert(!squad.IsEmpty(), "Members are only looked up for a named squad");

        for (const auto& [agent, name] : m_squadByAgent)
        {
            if (name == squad)
            {
                out.push_back(agent);
            }
        }
    }

    AZStd::vector<AZ::Name> SquadRegistry::GetNames() const
    {
        AZStd::vector<AZ::Name> names;
        names.reserve(m_squads.size());
        for (const auto& [name, squad] : m_squads)
        {
            names.push_back(name);
        }

        AZ_Assert(names.size() == m_squads.size(), "Listing squads must report exactly as many as exist");
        return names;
    }

    BlackboardStorage* SquadRegistry::FindStorage(const AZ::Name& squad)
    {
        const auto found = m_squads.find(squad);
        return found != m_squads.end() ? &found->second.m_storage : nullptr;
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
        AZ_Assert(squad != m_squads.end(), "A recorded membership must point at a squad that exists");
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

        AZ_Assert(m_squads.empty() && m_squadByAgent.empty(), "Clearing must leave no squad and no membership");
    }
} // namespace GOAT
