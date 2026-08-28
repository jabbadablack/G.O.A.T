#include <Core/Application/DecisionBackendAdapter.h>

#include <Core/Application/AgentRegistry.h>

#include <AzCore/Console/ILogger.h>

namespace GOAT
{
    DecisionBackendAdapter::DecisionBackendAdapter(
        IDecisionBackend& inner, AgentRegistry& agents, const ProgramTable& programs)
        : m_inner(inner)
        , m_agents(agents)
        , m_programs(programs)
    {
    }

    AZ::Name DecisionBackendAdapter::GetName() const
    {
        return m_inner.GetName();
    }

    bool DecisionBackendAdapter::Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan)
    {
        const auto found = m_programs.find(intent.m_goal);
        if (found == m_programs.end() || found->second == nullptr)
        {
            AZ_Warning("GOAT", false, "Backend '%s' was asked for '%s', which is not a compiled program",
                GetName().GetCStr(), intent.m_goal.GetCStr());
            return false;
        }

        const AgentProgram& program = *found->second;
        if (program.m_backend != &m_inner)
        {
            AZ_Warning("GOAT", false, "'%s' is not run by '%s', so it cannot be asked of it",
                intent.m_goal.GetCStr(), GetName().GetCStr());
            return false;
        }

        AgentRecord* record = m_agents.Find(context.m_agent);
        if (record == nullptr)
        {
            return false;
        }

        const size_t bytes = m_inner.GetStateSize();
        const BrainState state = record->BorrowState(bytes);
        if (bytes > 0 && state.empty())
        {
            AZ_Error("GOAT", false, "Agent %u has no room to ask '%s' for a plan",
                context.m_agent.GetIndex(), intent.m_goal.GetCStr());
            return false;
        }

        // Set up and torn down around the one question, because what this backend was in the
        // middle of is the host's business. Asked again, it starts again.
        m_inner.Attach(context, program, state);
        const Decision decision =
            m_inner.Decide(context, program, state, ActionResult::Success, 0.0f, outPlan);
        m_inner.Release(context, state);
        record->ReturnState(bytes);

        return decision.m_planned && !outPlan.IsEmpty();
    }
} // namespace GOAT
