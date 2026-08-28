#include <Core/Director/DirectorRegistry.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/std/algorithm.h>

namespace GOAT
{
    DirectorRegistry::DirectorRegistry(AgentRegistry& agents)
        : m_agents(agents)
    {
    }

    bool DirectorRegistry::Register(AgentId director, const DirectorProfile& profile)
    {
        AZ_Assert(!director.IsNull(), "A director is an agent, so it always has a handle");
        if (director.IsNull())
        {
            return false;
        }

        if (m_directors.contains(director))
        {
            AZ_Error("GOAT", false, "Agent %u is already a director", director.GetIndex());
            return false;
        }

        AZ_Warning("GOAT", profile.m_priority > SelfSwitchPriority,
            "Director %u has priority %u, which no agent switching itself can be outranked by",
            director.GetIndex(), static_cast<AZ::u32>(profile.m_priority));

        DirectorRecord record;
        record.m_profile = profile;
        m_directors.emplace(director, AZStd::move(record));

        AZ_Assert(FindProfile(director) != nullptr, "Registering a director must leave it findable");
        return true;
    }

    void DirectorRegistry::Unregister(AgentId director)
    {
        // Every cooldown this director owned goes with the record, which is the point of holding
        // them here rather than on the agents it was commanding.
        m_directors.erase(director);
    }

    bool DirectorRegistry::AttachFilter(AgentId director, IDirectorFilter& filter)
    {
        const auto found = m_directors.find(director);
        if (found == m_directors.end())
        {
            AZ_Error("GOAT", false, "Agent %u is not a director, so nothing can narrow it", director.GetIndex());
            return false;
        }

        AZStd::vector<IDirectorFilter*>& filters = found->second.m_filters;
        if (AZStd::find(filters.begin(), filters.end(), &filter) != filters.end())
        {
            return false;
        }

        filters.push_back(&filter);

        // The cached reach was resolved without this filter, so it has to be resolved again
        // rather than waiting out the band it was cached for.
        found->second.m_resolvedOnce = false;
        return true;
    }

    void DirectorRegistry::DetachFilter(AgentId director, IDirectorFilter& filter)
    {
        const auto found = m_directors.find(director);
        if (found == m_directors.end())
        {
            return;
        }

        AZStd::vector<IDirectorFilter*>& filters = found->second.m_filters;
        const auto at = AZStd::find(filters.begin(), filters.end(), &filter);
        if (at == filters.end())
        {
            return;
        }

        filters.erase(at);
        found->second.m_resolvedOnce = false;
    }

    AZStd::vector<const IDirectorFilter*> DirectorRegistry::GetFilters(AgentId director) const
    {
        const auto found = m_directors.find(director);
        if (found == m_directors.end())
        {
            return {};
        }

        return AZStd::vector<const IDirectorFilter*>(
            found->second.m_filters.begin(), found->second.m_filters.end());
    }

    const DirectorProfile* DirectorRegistry::FindProfile(AgentId director) const
    {
        const auto found = m_directors.find(director);
        return found != m_directors.end() ? &found->second.m_profile : nullptr;
    }

    AZStd::vector<AgentId> DirectorRegistry::GetDirectors() const
    {
        AZStd::vector<AgentId> directors;
        directors.reserve(m_directors.size());
        for (const auto& [director, record] : m_directors)
        {
            directors.push_back(director);
        }
        return directors;
    }

    const AZStd::vector<AgentId>& DirectorRegistry::Resolve(AgentId director)
    {
        const auto found = m_directors.find(director);
        if (found == m_directors.end())
        {
            return m_empty;
        }

        DirectorRecord& record = found->second;

        // The staleness budget is the director's own band interval: one evaluation per tick of
        // the thing doing the asking, which is what makes the cache self managing with no hook.
        const AgentRecord* self = m_agents.Find(director);
        const AZ::TimeMs interval = self != nullptr ? m_agents.GetBandInterval(self->m_band) : AZ::TimeMs{ 0 };
        const AZ::TimeMs now = AZ::GetElapsedTimeMs();

        if (record.m_resolvedOnce && now - record.m_resolvedAt < interval)
        {
            return record.m_reach;
        }

        Evaluate(director, record);
        SweepCooldowns(record);

        record.m_resolvedAt = now;
        record.m_resolvedOnce = true;
        return record.m_reach;
    }

    void DirectorRegistry::Evaluate(AgentId director, DirectorRecord& record)
    {
        record.m_reach.clear();

        // Slots, not agents: a released slot stays as a hole so every per agent index keeps
        // meaning the same thing, and a hole hands back a null handle to step over.
        const size_t slotCount = m_agents.GetSlotCount();
        for (size_t slot = 0; slot < slotCount; ++slot)
        {
            const AgentId candidate = m_agents.GetAgentAtSlot(slot);
            if (candidate.IsNull())
            {
                continue;
            }

            // A director never governs itself: it would then be able to order itself onto
            // another tree, which is a loop with no way out.
            if (candidate == director)
            {
                continue;
            }

            const AgentRecord* agent = m_agents.Find(candidate);
            if (agent == nullptr)
            {
                continue;
            }

            bool governed = true;
            for (const IDirectorFilter* filter : record.m_filters)
            {
                if (!filter->Accepts(candidate, agent->m_entity))
                {
                    governed = false;
                    break;
                }
            }

            if (governed)
            {
                record.m_reach.push_back(candidate);
            }
        }
    }

    void DirectorRegistry::SweepCooldowns(DirectorRecord& record)
    {
        // Walked here rather than on a timer: a cooldown for an agent that no longer exists can
        // never be asked about again, so it would otherwise sit in the map for the level's life.
        for (auto it = record.m_cooldowns.begin(); it != record.m_cooldowns.end();)
        {
            it = m_agents.Find(it->first.m_agent) == nullptr ? record.m_cooldowns.erase(it) : ++it;
        }
    }

    bool DirectorRegistry::IsOffCooldown(AgentId director, AgentId agent, ActionStateId verb) const
    {
        const auto found = m_directors.find(director);
        if (found == m_directors.end())
        {
            return false;
        }

        const auto timer = found->second.m_cooldowns.find(CooldownKey{ agent, verb });
        if (timer == found->second.m_cooldowns.end())
        {
            return true;
        }

        return AZ::GetElapsedTimeMs() >= timer->second;
    }

    void DirectorRegistry::StartCooldown(AgentId director, AgentId agent, ActionStateId verb)
    {
        const auto found = m_directors.find(director);
        AZ_Assert(found != m_directors.end(), "Only a director starts a cooldown");
        if (found == m_directors.end())
        {
            return;
        }

        const auto seconds = found->second.m_profile.m_cooldownSeconds;
        AZ_Assert(seconds >= 0.0f, "A cooldown cannot run backwards");

        found->second.m_cooldowns[CooldownKey{ agent, verb }] =
            AZ::GetElapsedTimeMs() + AZ::TimeMs{ static_cast<AZ::s64>(seconds * 1000.0f) };
    }
} // namespace GOAT
