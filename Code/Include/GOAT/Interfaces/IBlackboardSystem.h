#pragma once

#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/Domain/BlackboardStorage.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Name/Name.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/std/any.h>
#include <AzCore/std/string/string.h>

namespace GOAT
{
    //! The shared data every stage of the pipeline reads and writes.
    //! Holds one global blackboard, one per agent, and one per named squad.
    class IBlackboardSystem
    {
    public:
        AZ_RTTI(IBlackboardSystem, IBlackboardSystemTypeId);

        virtual ~IBlackboardSystem() = default;

        //! Declares a variable and assigns it a slot.
        //! Names are shared across every .bbx asset, so a duplicate is an error.
        virtual AZ::Outcome<BlackboardKey, AZStd::string> Declare(
            const AZ::Name& name, BlackboardScope scope, BlackboardType type, AZStd::any defaultValue = {}) = 0;

        //! Resolves a name to its key, or an invalid key when the name is undeclared.
        virtual BlackboardKey FindKey(const AZ::Name& name) const = 0;

        //! Names a key, for diagnostics. Scans, so it is not for a hot path.
        virtual AZ::Name GetKeyName(BlackboardKey key) const = 0;

        //! Creates the per agent storage used by agent scoped variables.
        virtual void CreateAgentBlackboard(AgentId agent) = 0;

        //! Destroys an agent's storage and drops it from its squad.
        virtual void DestroyAgentBlackboard(AgentId agent) = 0;

        //! Puts an agent in a named squad, creating that squad's storage on the first join.
        virtual void JoinSquad(AgentId agent, const AZ::Name& squad) = 0;

        //! Removes an agent from its squad, destroying that squad's storage on the last leave.
        virtual void LeaveSquad(AgentId agent) = 0;

        //! The squad an agent belongs to, or an empty name when it is in none.
        virtual AZ::Name GetSquad(AgentId agent) const = 0;

        //! Storage for a named squad, or nullptr when no such squad exists.
        //! Told apart from the agent keyed lookup below because a director writes to a squad it
        //! is not a member of, and has no agent in it to ask through.
        virtual BlackboardStorage* FindSquadStorage(const AZ::Name& squad) = 0;

        //! Every squad that currently has a member.
        virtual AZStd::vector<AZ::Name> GetSquadNames() const = 0;

        //! Storage backing one scope for one agent, or nullptr when it does not exist.
        //! Pass a null agent for global scope.
        virtual BlackboardStorage* FindStorage(BlackboardScope scope, AgentId agent) = 0;
        virtual const BlackboardStorage* FindStorage(BlackboardScope scope, AgentId agent) const = 0;

        //! Reads a variable through the storage its key names.
        template<typename T>
        const T* Find(BlackboardKey key, AgentId agent = {}) const
        {
            const BlackboardStorage* storage = FindStorage(key.GetScope(), agent);
            return storage != nullptr ? storage->Find<T>(key) : nullptr;
        }

        //! Writes a variable through the storage its key names.
        template<typename T>
        bool Set(BlackboardKey key, const T& value, AgentId agent = {})
        {
            BlackboardStorage* storage = FindStorage(key.GetScope(), agent);
            return storage != nullptr && storage->Set<T>(key, value);
        }

        //! Returns the change epoch for a specific key, or zero when the key/storage does not exist.
        virtual AZ::u32 GetKeyEpoch(BlackboardKey key, AgentId agent = {}) const = 0;
    };

    //! Registered by the GOAT system component for the lifetime of the gem.
    using BlackboardSystemInterface = AZ::Interface<IBlackboardSystem>;
} // namespace GOAT
