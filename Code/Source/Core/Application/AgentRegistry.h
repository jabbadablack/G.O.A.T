#pragma once

#include <Core/Application/AgentRecord.h>
#include <Core/Application/AgentRuntime.h>
#include <Core/Memory/HandleTable.h>

#include <AzCore/EBus/ScheduledEvent.h>
#include <AzCore/Time/ITime.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace GOAT
{
    //! Owns every registered agent and paces them through the engine's event scheduler.
    //! Agents are grouped into a few bands rather than given one scheduled event each, so
    //! the scheduler queue stays small while distant agents still run less often. The
    //! engine's own bg_maxScheduledEventProcessTimeMs supplies the per frame budget.
    class AgentRegistry final
    {
    public:
        //! How many pacing bands exist, from most to least frequent.
        static constexpr size_t BandCount = 4;

        AgentRegistry(AgentRuntime& runtime, IBlackboardSystem& blackboard, LuaDispatch& dispatch);
        ~AgentRegistry();

        //! Registers an entity as an agent running a compiled tree.
        AgentId Register(
            AZ::EntityId entity, const AZ::Name& treeName, AZStd::shared_ptr<const DecisionProgram> program, size_t band);

        //! Removes an agent, dropping its blackboard and its Lua scratch.
        void Unregister(AgentId agent);

        //! The record for an agent, or nullptr when the handle is stale.
        AgentRecord* Find(AgentId agent);

        //! Moves an agent to a different pacing band.
        void SetBand(AgentId agent, size_t band);

        //! Puts an agent onto another compiled tree, ending whatever it was running first.
        //! @param remember pushes the outgoing tree so a later pop returns to it.
        //! @return false when the agent is gone, or when its stack is already at its limit.
        bool ApplyTree(
            AgentId agent, const AZ::Name& treeName, AZStd::shared_ptr<const DecisionProgram> program, bool remember);

        //! Names the tree an agent should return to, or an empty name when it has none.
        AZ::Name PeekInterruptedTree(AgentId agent) const;

        //! Drops the innermost remembered tree, which a pop does once it has switched.
        void ForgetInterruptedTree(AgentId agent);

        //! How often each band runs.
        void SetBandIntervals(const AZStd::array<AZ::TimeMs, BandCount>& intervals);

        size_t Size() const { return m_agents.Size(); }

        //! Every live agent handle, for console output.
        AZStd::vector<AgentId> GetAgents() const;

    private:
        //! Runs every agent in one band and records when it last ran.
        void TickBand(size_t band);

        //! Takes an agent out of whichever band currently lists it.
        void RemoveFromBand(AgentId agent, size_t band);

        //! One pacing band: an interval, the agents on it, and its scheduler entry.
        struct Band final
        {
            AZ::TimeMs m_interval{ 100 };
            AZ::TimeMs m_lastTick{ 0 };
            AZStd::vector<AgentId> m_members;
            AZStd::unique_ptr<AZ::ScheduledEvent> m_event;
        };

        AgentRuntime& m_runtime;
        IBlackboardSystem& m_blackboard;
        LuaDispatch& m_dispatch;
        HandleTable<AZStd::unique_ptr<AgentRecord>, AgentTag> m_agents;
        AZStd::array<Band, BandCount> m_bands;
    };
} // namespace GOAT
