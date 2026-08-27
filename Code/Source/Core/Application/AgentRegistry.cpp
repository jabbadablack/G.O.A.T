#include <Core/Application/AgentRegistry.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/algorithm.h>

namespace GOAT
{
    AgentRegistry::AgentRegistry(AgentRuntime& runtime, IBlackboardSystem& blackboard, LuaDispatch& dispatch)
        : m_runtime(runtime)
        , m_blackboard(blackboard)
        , m_dispatch(dispatch)
    {
        // Defaults run close agents near frame rate and distant ones about once a second.
        const AZ::TimeMs defaults[BandCount] = { AZ::TimeMs{ 33 }, AZ::TimeMs{ 100 }, AZ::TimeMs{ 250 },
                                                 AZ::TimeMs{ 1000 } };

        for (size_t band = 0; band < BandCount; ++band)
        {
            m_bands[band].m_interval = defaults[band];
            m_bands[band].m_lastTick = AZ::GetElapsedTimeMs();
            m_bands[band].m_event = AZStd::make_unique<AZ::ScheduledEvent>(
                [this, band]()
                {
                    TickBand(band);
                },
                AZ::Name("GoatAgentBand"));
            m_bands[band].m_event->Enqueue(m_bands[band].m_interval, true);

            AZ_Assert(m_bands[band].m_interval > AZ::TimeMs{ 0 }, "An agent band must tick on a positive interval");
            AZ_Assert(m_bands[band].m_event->IsScheduled(), "An agent band's scheduled event must be queued");
        }
    }

    AgentRegistry::~AgentRegistry()
    {
        for (Band& band : m_bands)
        {
            if (band.m_event != nullptr)
            {
                band.m_event->RemoveFromQueue();
            }
        }
    }

    AgentId AgentRegistry::Register(
        AZ::EntityId entity,
        const AZ::Name& treeName,
        AZStd::shared_ptr<const DecisionProgram> program,
        size_t band,
        const AZ::Name& squad,
        AZStd::span<const AZ::Name> repertoire)
    {
        AZ_Assert(entity.IsValid(), "An agent must be registered against a valid entity");
        AZ_Assert(program != nullptr, "An agent must be registered with a compiled program");

        if (program == nullptr || program->IsEmpty())
        {
            AZ_Error("GOAT", false, "Entity %s cannot become an agent: its tree compiled to an empty program",
                entity.ToString().c_str());
            return AgentId{};
        }

        AZ_Warning("GOAT", band < BandCount, "LOD band %zu does not exist; entity %s falls back to the slowest band",
            band, entity.ToString().c_str());
        band = AZStd::min(band, BandCount - 1);

        auto record = AZStd::make_unique<AgentRecord>();
        AgentRecord* raw = record.get();
        const AgentId id = m_agents.Acquire(AZStd::move(record));

        raw->m_id = id;
        raw->m_entity = entity;
        raw->m_program = AZStd::move(program);
        raw->m_treeName = treeName;
        raw->m_band = band;
        raw->m_cursor.Reset(*raw->m_program);
        raw->m_wakeAt = 0.0f;

        // The tree it starts in is always one it may run, whatever was declared. Without this an
        // entity that listed nothing could never be returned to where it began.
        raw->m_repertoire.assign(repertoire.begin(), repertoire.end());
        if (!raw->MayRun(treeName))
        {
            raw->m_repertoire.push_back(treeName);
        }

        m_blackboard.CreateAgentBlackboard(id);

        // Squad membership before the observer connects, because the observer subscribes per
        // scope and skips one whose storage does not exist yet. Joining afterwards would leave
        // every squad scoped guard on this agent watching nothing.
        if (!squad.IsEmpty())
        {
            m_blackboard.JoinSquad(id, squad);
        }

        raw->m_observer.Connect(*raw->m_program, m_blackboard, id);

        AddToBand(id, band);
        m_byEntity[entity] = id;

        AZ_Assert(Find(id) == raw, "A registered agent must be findable by the id it was given");
        AZ_Assert(FindByEntity(entity) == id, "A registered agent must be findable by its entity");
        AZ_Assert(raw->m_band == band, "A registered agent must sit in the band it asked for");
        AZ_Assert(raw->MayRun(treeName), "An agent must be allowed to run the tree it starts in");

        AZLOG(GoatAgent, "GOAT: entity %s became agent %u in band %zu",
            entity.ToString().c_str(), id.GetIndex(), band);
        return id;
    }

    void AgentRegistry::AddToBand(AgentId agent, size_t band)
    {
        AZ_Assert(band < BandCount, "An agent can only join a band that exists");
        if (band >= BandCount)
        {
            return;
        }

        Band& entry = m_bands[band];
        if (entry.m_ticking)
        {
            entry.m_joining.push_back(agent);
            return;
        }

        entry.m_members.push_back(agent);
    }

    void AgentRegistry::RemoveFromBand(AgentId agent, size_t band)
    {
        AZ_Assert(band < BandCount, "An agent can only be removed from a band that exists");
        if (band >= BandCount)
        {
            return;
        }

        Band& entry = m_bands[band];
        if (entry.m_ticking)
        {
            entry.m_leaving.push_back(agent);
            return;
        }

        auto& members = entry.m_members;
        members.erase(AZStd::remove(members.begin(), members.end(), agent), members.end());

        AZ_Assert(AZStd::find(members.begin(), members.end(), agent) == members.end(),
            "Removing an agent from a band must leave no copy of it there");
    }

    void AgentRegistry::FlushBandChanges(size_t band)
    {
        Band& entry = m_bands[band];
        AZ_Assert(!entry.m_ticking, "Queued membership changes are applied once the band's tick has ended");

        // Removals first, so an agent that left and rejoined in one tick ends up present rather
        // than being erased by its own earlier departure.
        for (const AgentId agent : entry.m_leaving)
        {
            auto& members = entry.m_members;
            members.erase(AZStd::remove(members.begin(), members.end(), agent), members.end());
        }

        for (const AgentId agent : entry.m_joining)
        {
            entry.m_members.push_back(agent);
        }

        entry.m_leaving.clear();
        entry.m_joining.clear();
    }

    void AgentRegistry::Unregister(AgentId agent)
    {
        AgentRecord* record = Find(agent);
        AZ_Assert(record != nullptr, "Unregistering an agent that is not registered");
        if (record == nullptr)
        {
            return;
        }

        AZLOG(GoatAgent, "GOAT: agent %u is being unregistered", agent.GetIndex());

        // End whatever it was doing first. A running verb holds things it only gives back in End
        // -- a pooled path slot, a smart object claim, the block its plan borrowed -- so dropping
        // the record without this strands every one of them.
        m_runtime.AbortAgent(*record);

        // After the abort, so a backend is told the agent is gone only once its plan has been
        // given back and nothing can still be running through it.
        m_runtime.ReleaseAgent(*record);

        RemoveFromBand(agent, record->m_band);
        record->m_observer.Disconnect();

        // Drop the Lua scratch before the slot can be reused, so a new agent starts clean.
        m_dispatch.ForgetAgent(agent);
        m_byEntity.erase(record->m_entity);
        m_blackboard.DestroyAgentBlackboard(agent);
        m_agents.Release(agent);
    }

    AgentRecord* AgentRegistry::Find(AgentId agent)
    {
        AZStd::unique_ptr<AgentRecord>* found = m_agents.Find(agent);
        return found != nullptr ? found->get() : nullptr;
    }

    const AgentRecord* AgentRegistry::Find(AgentId agent) const
    {
        const AZStd::unique_ptr<AgentRecord>* found = m_agents.Find(agent);
        return found != nullptr ? found->get() : nullptr;
    }

    AgentId AgentRegistry::FindByEntity(AZ::EntityId entity) const
    {
        const auto found = m_byEntity.find(entity);
        return found != m_byEntity.end() ? found->second : AgentId{};
    }

    AZ::TimeMs AgentRegistry::GetBandInterval(size_t band) const
    {
        AZ_Assert(band < BandCount, "A band interval is only asked for a band that exists");
        return band < BandCount ? m_bands[band].m_interval : AZ::TimeMs{ 0 };
    }

    void AgentRegistry::SetBand(AgentId agent, size_t band)
    {
        AgentRecord* record = Find(agent);
        AZ_Assert(record != nullptr, "Changing the band of an agent that is not registered");
        if (record == nullptr)
        {
            AZ_Warning("GOAT", false, "Agent %u cannot change LOD band because it is not registered", agent.GetIndex());
            return;
        }

        band = AZStd::min(band, BandCount - 1);
        if (band == record->m_band)
        {
            return;
        }

        RemoveFromBand(agent, record->m_band);
        record->m_band = band;
        AddToBand(agent, band);

        AZ_Assert(record->m_band == band, "Changing band must record the band the agent moved to");
    }

    bool AgentRegistry::ApplyTree(
        AgentId agent, const AZ::Name& treeName, AZStd::shared_ptr<const DecisionProgram> program, bool remember)
    {
        AZ_Assert(!treeName.IsEmpty(), "An agent is only ever switched to a named tree");
        AZ_Assert(program != nullptr, "Switching a tree needs the compiled program to switch to");

        AgentRecord* record = Find(agent);
        AZ_Assert(record != nullptr, "Switching the tree of an agent that is not registered");
        if (record == nullptr || program == nullptr || program->IsEmpty())
        {
            return false;
        }

        if (remember)
        {
            if (record->m_treeStack.size() >= MaxTreeStackDepth)
            {
                AZ_Error("GOAT", false,
                    "Agent %u has interrupted itself %zu times without returning; that is a loop, "
                    "not a stack of behaviours",
                    agent.GetIndex(), record->m_treeStack.size());
                return false;
            }
            record->m_treeStack.push_back(record->m_treeName);
        }

        // Every step below is needed. Ending the running action is what gives back a pooled path
        // slot or a smart object claim; the cursor arrays are sized to the program; the observed
        // keys differ between programs; and the old intent names a node that no longer exists.
        m_runtime.AbortAgent(*record);

        // After the abort, so a backend is told the agent is gone only once its plan has been
        // given back and nothing can still be running through it.
        m_runtime.ReleaseAgent(*record);

        record->m_program = AZStd::move(program);
        record->m_treeName = treeName;
        record->m_cursor.Reset(*record->m_program);

        // A different tree is about to run, so whatever the previous one was waiting for says
        // nothing about this one. Zero means walk it on the next tick.
        record->m_wakeAt = 0.0f;

        record->m_observer.Disconnect();
        record->m_observer.Connect(*record->m_program, m_blackboard, agent);

        AZLOG(GoatAgent, "GOAT: agent %u is now running tree '%s' (%zu interrupted)",
            agent.GetIndex(), treeName.GetCStr(), record->m_treeStack.size());

        AZ_Assert(record->m_treeName == treeName, "Switching must leave the agent on the tree it asked for");
        AZ_Assert(!record->m_machine.HasPlan(), "Switching must leave the agent with no plan from the old tree");
        return true;
    }

    AZ::Name AgentRegistry::PeekInterruptedTree(AgentId agent) const
    {
        const auto* found = m_agents.Find(agent);
        const AgentRecord* record = found != nullptr ? found->get() : nullptr;
        if (record == nullptr || record->m_treeStack.empty())
        {
            return AZ::Name{};
        }
        return record->m_treeStack.back();
    }

    void AgentRegistry::ForgetInterruptedTree(AgentId agent)
    {
        AgentRecord* record = Find(agent);
        if (record == nullptr || record->m_treeStack.empty())
        {
            return;
        }

        record->m_treeStack.pop_back();
    }

    void AgentRegistry::JoinSquad(AgentId agent, const AZ::Name& squad)
    {
        AgentRecord* record = Find(agent);
        AZ_Assert(record != nullptr, "Only a registered agent can join a squad");
        if (record == nullptr)
        {
            return;
        }

        m_blackboard.JoinSquad(agent, squad);
        ReconnectObserver(*record);

        AZ_Assert(m_blackboard.GetSquad(agent) == squad, "Joining must leave the agent in that squad");
    }

    void AgentRegistry::LeaveSquad(AgentId agent)
    {
        AgentRecord* record = Find(agent);
        if (record == nullptr)
        {
            return;
        }

        m_blackboard.LeaveSquad(agent);
        ReconnectObserver(*record);

        AZ_Assert(m_blackboard.GetSquad(agent).IsEmpty(), "Leaving must leave the agent in no squad");
    }

    void AgentRegistry::ReconnectObserver(AgentRecord& record)
    {
        AZ_Assert(record.m_program != nullptr, "A registered agent always holds a compiled program");
        if (record.m_program == nullptr)
        {
            return;
        }

        // The observer subscribes per scope, so a scope whose storage did not exist at connect
        // time was skipped. Re-arming is the only way those guards ever start firing.
        record.m_observer.Disconnect();
        record.m_observer.Connect(*record.m_program, m_blackboard, record.m_id);
    }

    AZStd::vector<AgentId> AgentRegistry::GetAgents() const
    {
        AZStd::vector<AgentId> agents;
        agents.reserve(m_agents.Size());
        for (size_t i = 0; i < m_agents.Size(); ++i)
        {
            agents.push_back(m_agents.GetHandleAt(i));
        }
        return agents;
    }

    size_t AgentRegistry::GetAgentCount() const
    {
        return m_agents.Size();
    }

    AgentId AgentRegistry::GetAgentAt(size_t index) const
    {
        AZ_Assert(index < m_agents.Size(), "An agent index must address a registered agent");
        return m_agents.GetHandleAt(index);
    }

    void AgentRegistry::TickBand(size_t band)
    {
        AZ_Assert(band < BandCount, "A scheduled event fired for a band that does not exist");
        Band& entry = m_bands[band];

        // Measure the real gap rather than the nominal interval, so a band that the
        // engine's frame budget delayed still sees a correct delta time.
        const AZ::TimeMs now = AZ::GetElapsedTimeMs();
        const float deltaTime = AZStd::max(static_cast<float>(now - entry.m_lastTick) / 1000.0f, 0.0f);
        entry.m_lastTick = now;

        AZ_Assert(deltaTime >= 0.0f, "A band's delta time must never run backwards");

        // Walked in place. A behaviour that registers or removes an agent has its change queued
        // rather than applied, so the roster cannot move under this loop and nothing is copied.
        entry.m_ticking = true;
        for (size_t i = 0; i < entry.m_members.size(); ++i)
        {
            if (AgentRecord* record = Find(entry.m_members[i]))
            {
                m_runtime.Tick(*record, deltaTime);
            }
        }
        entry.m_ticking = false;

        FlushBandChanges(band);
    }
} // namespace GOAT
