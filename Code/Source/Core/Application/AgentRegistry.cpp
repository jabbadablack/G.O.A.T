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

    AgentId AgentRegistry::Register(AZ::EntityId entity, AZStd::shared_ptr<const DecisionProgram> program, size_t band)
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
        raw->m_band = band;
        raw->m_cursor.Reset(*raw->m_program);

        m_blackboard.CreateAgentBlackboard(id);
        raw->m_observer.Connect(*raw->m_program, m_blackboard, id);

        m_bands[band].m_members.push_back(id);

        AZ_Assert(Find(id) == raw, "A registered agent must be findable by the id it was given");
        AZ_Assert(raw->m_band == band, "A registered agent must sit in the band it asked for");

        AZLOG(GoatAgent, "GOAT: entity %s became agent %u in band %zu",
            entity.ToString().c_str(), id.GetIndex(), band);
        return id;
    }

    void AgentRegistry::RemoveFromBand(AgentId agent, size_t band)
    {
        AZ_Assert(band < BandCount, "An agent can only be removed from a band that exists");
        if (band >= BandCount)
        {
            return;
        }

        auto& members = m_bands[band].m_members;
        members.erase(AZStd::remove(members.begin(), members.end(), agent), members.end());

        AZ_Assert(AZStd::find(members.begin(), members.end(), agent) == members.end(),
            "Removing an agent from a band must leave no copy of it there");
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

        RemoveFromBand(agent, record->m_band);
        record->m_observer.Disconnect();

        // Drop the Lua scratch before the slot can be reused, so a new agent starts clean.
        m_dispatch.ForgetAgent(agent);
        m_blackboard.DestroyAgentBlackboard(agent);
        m_agents.Release(agent);

        AZ_Assert(Find(agent) == nullptr, "An unregistered agent must no longer be findable");
    }

    AgentRecord* AgentRegistry::Find(AgentId agent)
    {
        AZStd::unique_ptr<AgentRecord>* found = m_agents.Find(agent);
        return found != nullptr ? found->get() : nullptr;
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
        m_bands[band].m_members.push_back(agent);

        AZ_Assert(record->m_band == band, "Changing band must record the band the agent moved to");
    }

    void AgentRegistry::SetBandIntervals(const AZStd::array<AZ::TimeMs, BandCount>& intervals)
    {
        for (size_t band = 0; band < BandCount; ++band)
        {
            AZ_Assert(intervals[band] > AZ::TimeMs{ 0 }, "An agent band interval must be positive");

            m_bands[band].m_interval = intervals[band];
            AZ_Assert(m_bands[band].m_event != nullptr, "Every agent band owns a scheduled event for its lifetime");
            if (m_bands[band].m_event != nullptr)
            {
                m_bands[band].m_event->Requeue(intervals[band]);
            }
        }
    }

    AZStd::vector<AgentId> AgentRegistry::GetAgents() const
    {
        AZStd::vector<AgentId> agents;
        agents.reserve(m_agents.Size());
        const size_t expected = m_agents.Size();
        for (size_t i = 0; i < m_agents.Size(); ++i)
        {
            agents.push_back(m_agents.GetHandleAt(i));
        }

        AZ_Assert(agents.size() == expected, "Listing agents must report exactly as many as are registered");
        return agents;
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

        // Copy the roster: a behaviour may register or remove agents while it runs.
        AZStd::vector<AgentId> roster = entry.m_members;
        for (const AgentId agent : roster)
        {
            if (AgentRecord* record = Find(agent))
            {
                m_runtime.Tick(*record, deltaTime);
            }
        }
    }
} // namespace GOAT
