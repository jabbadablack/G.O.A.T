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

        //! Every agent in a squad, appended to @out.
        //! The reverse of Find, which a director governing a squad it is not a member of has no
        //! other way to ask for. Walks the membership map rather than keeping a second index:
        //! one map is one invariant, and the walk costs what the roster walk it replaces costs.
        void FindMembers(const AZ::Name& squad, AZStd::vector<AgentId>& out) const;

        //! Every squad that currently has a member, for console output.
        AZStd::vector<AZ::Name> GetNames() const;

        //! Storage for a named squad, or nullptr when no such squad exists.
        //! Told apart from the agent keyed overload because a director writes to a squad it is
        //! not a member of, and so has no agent in it to ask through.
        BlackboardStorage* FindStorage(const AZ::Name& squad);

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
