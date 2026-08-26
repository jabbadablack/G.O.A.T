#include <Core/Application/AgentRegistry.h>

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
        if (program == nullptr || program->IsEmpty())
        {
            return AgentId{};
        }

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
        return id;
    }

    void AgentRegistry::RemoveFromBand(AgentId agent, size_t band)
    {
        if (band >= BandCount)
        {
            return;
        }

        auto& members = m_bands[band].m_members;
        members.erase(AZStd::remove(members.begin(), members.end(), agent), members.end());
    }

    void AgentRegistry::Unregister(AgentId agent)
    {
        AgentRecord* record = Find(agent);
        if (record == nullptr)
        {
            return;
        }

        RemoveFromBand(agent, record->m_band);
        record->m_observer.Disconnect();

        // Drop the Lua scratch before the slot can be reused, so a new agent starts clean.
        m_dispatch.ForgetAgent(agent);
        m_blackboard.DestroyAgentBlackboard(agent);
        m_agents.Release(agent);
    }

    AgentRecord* AgentRegistry::Find(AgentId agent)
    {
        AZStd::unique_ptr<AgentRecord>* found = m_agents.Find(agent);
        return found != nullptr ? found->get() : nullptr;
    }

    void AgentRegistry::SetBand(AgentId agent, size_t band)
    {
        AgentRecord* record = Find(agent);
        if (record == nullptr)
        {
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
    }

    void AgentRegistry::SetBandIntervals(const AZStd::array<AZ::TimeMs, BandCount>& intervals)
    {
        for (size_t band = 0; band < BandCount; ++band)
        {
            m_bands[band].m_interval = intervals[band];
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
        for (size_t i = 0; i < m_agents.Size(); ++i)
        {
            agents.push_back(m_agents.GetHandleAt(i));
        }
        return agents;
    }

    void AgentRegistry::TickBand(size_t band)
    {
        Band& entry = m_bands[band];

        // Measure the real gap rather than the nominal interval, so a band that the
        // engine's frame budget delayed still sees a correct delta time.
        const AZ::TimeMs now = AZ::GetElapsedTimeMs();
        const float deltaTime = AZStd::max(static_cast<float>(now - entry.m_lastTick) / 1000.0f, 0.0f);
        entry.m_lastTick = now;

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
