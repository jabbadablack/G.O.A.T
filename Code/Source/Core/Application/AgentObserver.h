#pragma once

#include <GOAT/Domain/BlackboardStorage.h>
#include <GOAT/Domain/DecisionProgram.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Watches only the blackboard slots an agent's tree actually guards on.
    //! This is what lets an agent with nothing changing evaluate no conditions at all,
    //! instead of re-checking its guards every tick.
    class AgentObserver final
    {
    public:
        //! Subscribes to the storages the program's observed keys live in.
        void Connect(const DecisionProgram& program, IBlackboardSystem& blackboard, AgentId agent);

        //! Drops every subscription, for example when an agent changes squad or tree.
        void Disconnect();

        //! True when a watched slot changed since the last Clear.
        bool IsDirty() const { return m_dirty; }

        //! Marks the agent as needing a guard re-check on its next tick.
        void MarkDirty() { m_dirty = true; }

        //! Called once the guards have been re-checked.
        void Clear() { m_dirty = false; }

    private:
        //! Marks the agent when the changed slot is one this tree guards on.
        void OnChanged(BlackboardKey key);

        //! Slots this tree guards on, sorted so membership is a binary search.
        AZStd::vector<BlackboardKey> m_observed;
        AZStd::array<BlackboardStorage::ChangedEvent::Handler, static_cast<size_t>(BlackboardScope::Count)> m_handlers;
        //! Starts dirty so the first tick evaluates the guards once.
        bool m_dirty = true;
    };
} // namespace GOAT
