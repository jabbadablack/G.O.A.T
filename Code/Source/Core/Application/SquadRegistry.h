#pragma once

#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/BlackboardLayout.h>
#include <GOAT/Domain/BlackboardStorage.h>

#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/utils.h>

namespace GOAT
{
    //! Named agent groups, each owning the blackboard storage for squad scoped variables.
    //! A squad exists only while it has members: created on the first join, destroyed on the last leave.
    class SquadRegistry final
    {
    public:
        //! Adds an agent to a squad, leaving whatever squad it was in first.
        void Join(AgentId agent, const AZ::Name& squad, const BlackboardLayout& layout);

        //! Removes an agent from its squad. Does nothing when it is in none.
        void Leave(AgentId agent);

        //! The squad an agent belongs to, or an empty name.
        AZ::Name Find(AgentId agent) const;

        //! Storage for the squad an agent belongs to, or nullptr when it is in none.
        BlackboardStorage* FindStorage(AgentId agent);
        const BlackboardStorage* FindStorage(AgentId agent) const;

        //! Grows every live squad's storage to a new layout.
        void EnsureCapacity(const BlackboardLayout& layout);

        //! Removes every squad and every membership.
        void Clear();

    private:
        //! One squad's storage plus the member count that keeps it alive.
        struct Squad
        {
            BlackboardStorage m_storage;
            AZ::u32 m_memberCount = 0;
        };

        AZStd::unordered_map<AZ::Name, Squad> m_squads;
        AZStd::unordered_map<AgentId, AZ::Name> m_squadByAgent;
    };
} // namespace GOAT
