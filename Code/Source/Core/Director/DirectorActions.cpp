#include <Core/Director/DirectorActions.h>

#include <GOAT/Interfaces/IAgentSystem.h>
#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

namespace GOAT
{
    namespace
    {
        //! The agents a verb should act on: one named through the blackboard, or the whole reach.
        //!
        //! `key` names an EntityId variable. Set and holding an agent in reach, the verb commands
        //! that one; unset, it commands them all. That is per agent and per group granularity
        //! with no extra vocabulary, using the blackboard as the channel it already is.
        void SelectTargets(
            const ActionContext& context,
            const AZStd::vector<AgentId>& reach,
            AZStd::vector<AgentId>& out)
        {
            out.clear();

            const BlackboardKey key = context.m_request->m_targetKey;
            if (!key.IsValid())
            {
                out = reach;
                return;
            }

            const AZ::EntityId* named = context.m_blackboard->Find<AZ::EntityId>(key, context.m_agent);
            if (named == nullptr || !named->IsValid())
            {
                // The variable names nobody yet, which is a normal state for a director that has
                // not chosen a target this tick, not a mistake worth warning about.
                return;
            }

            IAgentSystem* agents = AgentSystemInterface::Get();
            const AgentId wanted = agents != nullptr ? agents->FindAgent(*named) : AgentId{};

            for (const AgentId candidate : reach)
            {
                if (candidate == wanted)
                {
                    out.push_back(candidate);
                    return;
                }
            }
        }

        //! How many agents a verb may act on, from its `limit` property. Zero means all of them.
        //! A limit is what lets a director escalate rather than flip a whole population at once.
        size_t ReadLimit(const ActionContext& context, size_t available)
        {
            const float limit = context.m_request->m_amount;
            if (limit <= 0.0f)
            {
                return available;
            }
            return AZStd::min(available, static_cast<size_t>(limit));
        }

        //! Records what a verb did on the director's own blackboard.
        void Report(const ActionContext& context, const DirectorKeys& keys, size_t reach, size_t changed, size_t refused)
        {
            AZ_Assert(keys.IsValid(), "Reporting needs the director variables declared");

            context.m_blackboard->Set<AZ::s64>(keys.m_reach, static_cast<AZ::s64>(reach), context.m_agent);
            context.m_blackboard->Set<AZ::s64>(keys.m_changed, static_cast<AZ::s64>(changed), context.m_agent);
            context.m_blackboard->Set<AZ::s64>(keys.m_refused, static_cast<AZ::s64>(refused), context.m_agent);
        }
    } // namespace

    DirectorActionBase::DirectorActionBase(DirectorRegistry& directors, const DirectorKeys& keys)
        : m_directors(directors)
        , m_keys(keys)
    {
    }

    void DirectorActionBase::Begin([[maybe_unused]] const ActionContext& context)
    {
    }

    void DirectorActionBase::End([[maybe_unused]] const ActionContext& context)
    {
    }

    ActionStateId DirectorActionBase::GetVerbId(const ActionContext& context) const
    {
        AZ_Assert(context.m_request != nullptr, "A verb always runs with the request that named it");
        return context.m_request->m_action;
    }

    ActionResult DirectorActionBase::Step(const ActionContext& context, [[maybe_unused]] float deltaTime)
    {
        AZ_Assert(context.m_request != nullptr, "A director verb always runs with a request");
        AZ_Assert(context.m_blackboard != nullptr, "A director verb always runs with a blackboard");

        const AgentId director = context.m_agent;
        if (m_directors.FindProfile(director) == nullptr)
        {
            AZ_Error("GOAT", false,
                "Agent %u ran a director verb but is not a director; that word belongs in a tree on "
                "a GOAT Director component",
                director.GetIndex());
            return ActionResult::Failure;
        }

        const AZStd::vector<AgentId>& reach = m_directors.Resolve(director);

        AZStd::vector<AgentId> targets;
        SelectTargets(context, reach, targets);

        const size_t limit = ReadLimit(context, targets.size());
        const ActionStateId verb = GetVerbId(context);

        size_t changed = 0;
        size_t refused = 0;

        for (size_t i = 0; i < targets.size() && changed < limit; ++i)
        {
            const AgentId agent = targets[i];

            // Idempotence is checked inside Apply, before the cooldown is consulted, so an order
            // that would change nothing neither spends a cooldown nor starts one.
            if (UsesCooldown() && !m_directors.IsOffCooldown(director, agent, verb))
            {
                ++refused;
                continue;
            }

            if (Apply(context, director, agent) == Outcome::Changed)
            {
                ++changed;
                if (UsesCooldown())
                {
                    m_directors.StartCooldown(director, agent, verb);
                }
            }
            else
            {
                ++refused;
            }
        }

        Report(context, m_keys, reach.size(), changed, refused);

        // Success means something is different than it was. A tree branches on that with the
        // selector it already has, which is the whole refusal reporting mechanism.
        return changed > 0 ? ActionResult::Success : ActionResult::Failure;
    }

    AZ::Name OrderTreeAction::GetName() const
    {
        return AZ_NAME_LITERAL("order_tree");
    }

    DirectorActionBase::Outcome OrderTreeAction::Apply(
        const ActionContext& context, AgentId director, AgentId agent)
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        const AZ::Name wanted = context.m_request->m_tag;
        AZ_Assert(!wanted.IsEmpty(), "An order_tree leaf always names the tree it orders");

        if (agents == nullptr || wanted.IsEmpty())
        {
            return Outcome::Refused;
        }

        if (agents->GetAgentTree(agent) == wanted)
        {
            return Outcome::Refused;
        }

        const DirectorProfile* profile = m_directors.FindProfile(director);
        AZ_Assert(profile != nullptr, "Apply is only reached for a registered director");

        return agents->SetAgentTree(agent, wanted, profile != nullptr ? profile->m_priority : SelfSwitchPriority)
            ? Outcome::Changed
            : Outcome::Refused;
    }

    AZ::Name OrderInterruptAction::GetName() const
    {
        return AZ_NAME_LITERAL("order_interrupt");
    }

    DirectorActionBase::Outcome OrderInterruptAction::Apply(
        const ActionContext& context, AgentId director, AgentId agent)
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        const AZ::Name wanted = context.m_request->m_tag;
        AZ_Assert(!wanted.IsEmpty(), "An order_interrupt leaf always names the tree it orders");

        if (agents == nullptr || wanted.IsEmpty())
        {
            return Outcome::Refused;
        }

        // Interrupting an agent with what it is already running would stack the same tree on
        // itself, which is a push it could never usefully return from.
        if (agents->GetAgentTree(agent) == wanted)
        {
            return Outcome::Refused;
        }

        const DirectorProfile* profile = m_directors.FindProfile(director);
        return agents->PushAgentTree(agent, wanted, profile != nullptr ? profile->m_priority : SelfSwitchPriority)
            ? Outcome::Changed
            : Outcome::Refused;
    }

    AZ::Name OrderBandAction::GetName() const
    {
        return AZ_NAME_LITERAL("order_band");
    }

    DirectorActionBase::Outcome OrderBandAction::Apply(
        const ActionContext& context, [[maybe_unused]] AgentId director, AgentId agent)
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        if (agents == nullptr)
        {
            return Outcome::Refused;
        }

        const size_t wanted = static_cast<size_t>(AZStd::max(context.m_request->m_amount, 0.0f));
        if (agents->GetAgentBand(agent) == wanted)
        {
            return Outcome::Refused;
        }

        return agents->SetAgentBand(agent, wanted) ? Outcome::Changed : Outcome::Refused;
    }

    OrderValueAction::OrderValueAction(DirectorRegistry& directors, const DirectorKeys& keys)
        : m_directors(directors)
        , m_keys(keys)
    {
    }

    AZ::Name OrderValueAction::GetName() const
    {
        return AZ_NAME_LITERAL("order_value");
    }

    void OrderValueAction::Begin([[maybe_unused]] const ActionContext& context)
    {
    }

    void OrderValueAction::End([[maybe_unused]] const ActionContext& context)
    {
    }

    ActionResult OrderValueAction::Step(const ActionContext& context, [[maybe_unused]] float deltaTime)
    {
        AZ_Assert(context.m_request != nullptr, "A director verb always runs with a request");

        const AgentId director = context.m_agent;
        if (m_directors.FindProfile(director) == nullptr)
        {
            AZ_Error("GOAT", false, "Agent %u ran order_value but is not a director", director.GetIndex());
            return ActionResult::Failure;
        }

        const BlackboardKey key = context.m_request->m_targetKey;
        AZ_Assert(key.IsValid(), "An order_value leaf always names a declared variable");
        if (!key.IsValid())
        {
            return ActionResult::Failure;
        }

        const AZStd::vector<AgentId>& reach = m_directors.Resolve(director);

        //! Writes one storage, counting only a write that actually changed something.
        //! Set reports that the key was valid, not that the value moved, so the comparison has
        //! to happen here.
        const auto write = [&context, key](AgentId through) -> bool
        {
            switch (key.GetType())
            {
            case BlackboardType::Bool:
            {
                const bool wanted = context.m_request->m_amount != 0.0f;
                const bool* current = context.m_blackboard->Find<bool>(key, through);
                if (current != nullptr && *current == wanted)
                {
                    return false;
                }
                return context.m_blackboard->Set<bool>(key, wanted, through);
            }
            case BlackboardType::Int:
            {
                const auto wanted = static_cast<AZ::s64>(context.m_request->m_amount);
                const AZ::s64* current = context.m_blackboard->Find<AZ::s64>(key, through);
                if (current != nullptr && *current == wanted)
                {
                    return false;
                }
                return context.m_blackboard->Set<AZ::s64>(key, wanted, through);
            }
            case BlackboardType::Float:
            {
                const float wanted = context.m_request->m_amount;
                const float* current = context.m_blackboard->Find<float>(key, through);
                if (current != nullptr && *current == wanted)
                {
                    return false;
                }
                return context.m_blackboard->Set<float>(key, wanted, through);
            }
            case BlackboardType::Name:
            {
                const AZ::Name wanted = context.m_request->m_tag;
                const AZ::Name* current = context.m_blackboard->Find<AZ::Name>(key, through);
                if (current != nullptr && *current == wanted)
                {
                    return false;
                }
                return context.m_blackboard->Set<AZ::Name>(key, wanted, through);
            }
            default:
                AZ_Error("GOAT", false,
                    "order_value cannot write a %s variable; a position is exact truth, and the "
                    "director channel is meant to be lossy",
                    ToString(key.GetType()));
                return false;
            }
        };

        size_t changed = 0;

        // Which storages a write reaches is decided by the variable's declared scope, which is
        // why this verb needs no parameter saying so.
        switch (key.GetScope())
        {
        case BlackboardScope::Global:
            // Global is global: the reach is irrelevant, and writing it once is the whole job.
            changed = write(AgentId{}) ? 1 : 0;
            break;

        case BlackboardScope::Squad:
        {
            // Once per squad represented in reach, not once per agent, or every member after the
            // first would compare equal to what the first already wrote.
            AZStd::vector<AZ::Name> written;
            IAgentSystem* agents = AgentSystemInterface::Get();
            for (const AgentId agent : reach)
            {
                const AZ::Name squad = agents != nullptr ? agents->GetAgentSquad(agent) : AZ::Name{};
                if (squad.IsEmpty() || AZStd::find(written.begin(), written.end(), squad) != written.end())
                {
                    continue;
                }

                written.push_back(squad);
                changed += write(agent) ? 1 : 0;
            }
            break;
        }

        case BlackboardScope::Agent:
            for (const AgentId agent : reach)
            {
                changed += write(agent) ? 1 : 0;
            }
            break;

        default:
            break;
        }

        Report(context, m_keys, reach.size(), changed, reach.size() - changed);
        return changed > 0 ? ActionResult::Success : ActionResult::Failure;
    }

    RebindSubtreeAction::RebindSubtreeAction(const DirectorKeys& keys)
        : m_keys(keys)
    {
    }

    AZ::Name RebindSubtreeAction::GetName() const
    {
        return AZ_NAME_LITERAL("rebind_subtree");
    }

    void RebindSubtreeAction::Begin([[maybe_unused]] const ActionContext& context)
    {
    }

    void RebindSubtreeAction::End([[maybe_unused]] const ActionContext& context)
    {
    }

    ActionResult RebindSubtreeAction::Step(const ActionContext& context, [[maybe_unused]] float deltaTime)
    {
        IAgentSystem* agents = AgentSystemInterface::Get();
        const AZ::Name slot = context.m_request->m_tag;
        AZ_Assert(!slot.IsEmpty(), "A rebind_subtree leaf always names the slot it rebinds");

        if (agents == nullptr || slot.IsEmpty())
        {
            return ActionResult::Failure;
        }

        // Two names are needed and a leaf carries one, so the tree to bind arrives through the
        // blackboard. The choice is made in Lua, where behaviour belongs; the mechanism is here.
        const BlackboardKey key = context.m_request->m_targetKey;
        const AZ::Name* wanted = key.IsValid()
            ? context.m_blackboard->Find<AZ::Name>(key, context.m_agent)
            : nullptr;

        if (wanted == nullptr || wanted->IsEmpty())
        {
            AZ_Warning("GOAT", false,
                "rebind_subtree found no tree name to bind to slot '%s'; it reads one from the "
                "variable its key names",
                slot.GetCStr());
            return ActionResult::Failure;
        }

        auto rebound = agents->RebindSubtree(slot, *wanted);
        if (!rebound.IsSuccess())
        {
            AZ_Error("GOAT", false, "%s", rebound.GetError().c_str());
            return ActionResult::Failure;
        }

        // Reported in the same three variables the other verbs use, reading here as "how many
        // trees the slot reached, and how many of them this rebind rewrote".
        const size_t recompiled = rebound.GetValue();
        Report(context, m_keys, recompiled, recompiled, 0);

        // A rebind that recompiled nothing changed nothing, which is a failure the same way an
        // order nobody accepted is.
        return recompiled > 0 ? ActionResult::Success : ActionResult::Failure;
    }
} // namespace GOAT
