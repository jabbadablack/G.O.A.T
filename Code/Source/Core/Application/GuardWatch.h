#pragma once

#include <GOAT/Domain/BlackboardStorage.h>
#include <GOAT/Domain/DecisionProgram.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/std/containers/array.h>

namespace GOAT
{
    //! Notices when anything an agent's tree guards on has changed, without subscribing to it.
    //!
    //! Each blackboard scope counts its own changes, and an agent remembers the count it last
    //! acted on. A write is then one increment rather than one callback per agent watching that
    //! scope, which is what stops a single global write costing a walk of the whole level. It
    //! also means this holds no handler and captures no address, so the record it lives in is
    //! free to move.
    //!
    //! The cost is that a scope is the finest thing counted: any change to a scope an agent
    //! watches wakes it, not only a change to the slot it guards on. That is the right trade for
    //! an idle agent, which by definition is running nothing that writes.
    class GuardWatch final
    {
    public:
        //! Points at the storage of each scope this tree guards on. A tree with no guards
        //! watches nothing and is never woken by a write.
        void Connect(const DecisionProgram& program, IBlackboardSystem& blackboard, AgentId agent);

        //! Stops watching, for example when an agent changes squad or tree.
        void Disconnect();

        //! True when a watched scope has changed since the last Clear.
        bool IsDirty() const;

        //! Marks the agent as needing a guard re-check on its next tick.
        void MarkDirty() { m_forced = true; }

        //! Called once the guards have been re-checked.
        void Clear();

    private:
        static constexpr size_t ScopeCount = static_cast<size_t>(BlackboardScope::Count);

        //! Storage of each watched scope, or null for a scope this tree does not guard on.
        //! Safe to hold because a storage instance lives in a node based map and so keeps its
        //! address, and because an agent stops watching before its storage is destroyed.
        AZStd::array<const BlackboardStorage*, ScopeCount> m_watched{};

        //! The change count this agent has already accounted for, per watched scope.
        AZStd::array<AZ::u32, ScopeCount> m_seen{};

        //! Set when something outside a blackboard write says the guards are stale, and on the
        //! first tick, because a freshly connected agent has never evaluated them.
        bool m_forced = true;
    };
} // namespace GOAT
