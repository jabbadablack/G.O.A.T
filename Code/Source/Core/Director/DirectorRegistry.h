#pragma once

#include <Core/Application/AgentRegistry.h>

#include <GOAT/Domain/ActionState.h>
#include <GOAT/Domain/AgentId.h>
#include <GOAT/Domain/DirectorProfile.h>
#include <GOAT/Interfaces/IDirectorFilter.h>

#include <AzCore/Time/ITime.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! Which directors exist, who each one governs, and when it may command them again.
    //!
    //! Keyed by the director's own AgentId, because a director is an agent: a verb running on its
    //! tree already holds that id in its ActionContext, so looking itself up costs nothing and the
    //! handle is generation checked, meaning a stale director can never alias a new one.
    class DirectorRegistry final
    {
    public:
        explicit DirectorRegistry(AgentRegistry& agents);

        //! Makes an agent a director. Fails when it already is one.
        bool Register(AgentId director, const DirectorProfile& profile);
        void Unregister(AgentId director);

        //! The profile of a director, or nullptr when that agent is not one.
        const DirectorProfile* FindProfile(AgentId director) const;

        //! Every director, for console output.
        AZStd::vector<AgentId> GetDirectors() const;

        //! Narrows what a director governs. The filter is not owned: the component attaching it
        //! is the filter, and it detaches before it goes.
        bool AttachFilter(AgentId director, IDirectorFilter& filter);
        void DetachFilter(AgentId director, IDirectorFilter& filter);

        //! The filters narrowing a director, for console output.
        AZStd::vector<const IDirectorFilter*> GetFilters(AgentId director) const;

        //! The agents a director governs.
        //!
        //! Resolved once per director tick and cached until its band comes round again. Every
        //! verb and every sensing call inside one tick has to see the same set: a set that
        //! changed underneath a Lua loop would let it address an agent a later verb no longer
        //! governs.
        const AZStd::vector<AgentId>& Resolve(AgentId director);

        //! True when this director may command that agent with that verb now.
        //! Only the timer. Whether the command would change anything is the verb's own question,
        //! and it must be asked first so a no-op neither consumes nor starts a cooldown.
        bool IsOffCooldown(AgentId director, AgentId agent, ActionStateId verb) const;

        //! Starts this director's cooldown on that agent for that verb.
        void StartCooldown(AgentId director, AgentId agent, ActionStateId verb);

    private:
        //! One director's cooldown on one agent for one verb.
        //! Keyed by the agent as well as the verb because a cooldown is *this* director's
        //! relationship with that agent: another director must still be able to command it, or
        //! one director's order would silence another's by accident rather than by priority.
        struct CooldownKey final
        {
            AgentId m_agent;
            ActionStateId m_verb = CoreActions::Invalid;

            bool operator==(const CooldownKey& rhs) const
            {
                return m_agent == rhs.m_agent && m_verb == rhs.m_verb;
            }
        };

        struct CooldownHash final
        {
            size_t operator()(const CooldownKey& key) const
            {
                return AZStd::hash<AgentId>{}(key.m_agent) ^ (static_cast<size_t>(key.m_verb) << 24);
            }
        };

        struct DirectorRecord final
        {
            DirectorProfile m_profile;
            //! What narrows this director. Empty means it governs the whole level.
            AZStd::vector<IDirectorFilter*> m_filters;
            //! The agents governed as of m_resolvedAt.
            AZStd::vector<AgentId> m_reach;
            AZ::TimeMs m_resolvedAt{ 0 };
            bool m_resolvedOnce = false;
            AZStd::unordered_map<CooldownKey, AZ::TimeMs, CooldownHash> m_cooldowns;
        };

        //! Fills a record's reach from the roster: every agent but this one, less what any
        //! attached filter rejects.
        void Evaluate(AgentId director, DirectorRecord& record);

        //! Drops cooldown entries for agents that are gone, while already walking the roster.
        void SweepCooldowns(DirectorRecord& record);

        AgentRegistry& m_agents;
        AZStd::unordered_map<AgentId, DirectorRecord> m_directors;

        //! Returned for an agent that is not a director, so Resolve can hand back a reference.
        AZStd::vector<AgentId> m_empty;
    };
} // namespace GOAT
