#include <Core/Application/GuardWatch.h>

#include <AzCore/Debug/Trace.h>
#include <AzCore/std/algorithm.h>

namespace GOAT
{
    void GuardWatch::Connect(const DecisionProgram& program, IBlackboardSystem& blackboard, AgentId agent)
    {
        Disconnect();

        for (size_t scopeIndex = 0; scopeIndex < ScopeCount; ++scopeIndex)
        {
            const auto scope = static_cast<BlackboardScope>(scopeIndex);
            const bool watchesScope = AZStd::any_of(
                program.m_observedKeys.begin(), program.m_observedKeys.end(),
                [scope](BlackboardKey key)
                {
                    return key.GetScope() == scope;
                });

            if (!watchesScope)
            {
                continue;
            }

            const BlackboardStorage* storage = blackboard.FindStorage(scope, agent);
            AZ_Warning("GOAT", storage != nullptr,
                "Agent %u guards a variable in a scope it has no storage for, so those guards never fire",
                agent.GetIndex());

            m_watched[scopeIndex] = storage;
        }

        // A freshly connected agent has never evaluated its guards.
        m_forced = true;
    }

    void GuardWatch::Disconnect()
    {
        m_watched.fill(nullptr);
        m_seen.fill(0);
        m_forced = true;
    }

    bool GuardWatch::IsDirty() const
    {
        if (m_forced)
        {
            return true;
        }

        for (size_t scopeIndex = 0; scopeIndex < ScopeCount; ++scopeIndex)
        {
            const BlackboardStorage* storage = m_watched[scopeIndex];
            if (storage != nullptr && storage->GetEpoch() != m_seen[scopeIndex])
            {
                return true;
            }
        }

        return false;
    }

    void GuardWatch::Clear()
    {
        m_forced = false;
        for (size_t scopeIndex = 0; scopeIndex < ScopeCount; ++scopeIndex)
        {
            if (m_watched[scopeIndex] != nullptr)
            {
                m_seen[scopeIndex] = m_watched[scopeIndex]->GetEpoch();
            }
        }
    }
} // namespace GOAT
