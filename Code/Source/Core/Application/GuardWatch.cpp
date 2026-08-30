#include <Core/Application/GuardWatch.h>
#include <Backends/BehaviorTree/Code/Include/GOAT_BehaviorTree/DecisionProgram.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT
{
    void GuardWatch::Connect(const AgentProgram& program, IBlackboardSystem& blackboard, AgentId agent)
    {
        Disconnect();

        // Prefer per-key watching when the concrete program supplies observed keys (DecisionProgram).
        const DecisionProgram* dprog = azrtti_cast<const DecisionProgram*>(&program);
        if (dprog != nullptr && !dprog->m_observedKeys.empty())
        {
            m_observedKeys = dprog->m_observedKeys;
            m_seenKeyEpochs.resize(m_observedKeys.size());
            m_blackboard = &blackboard;
            m_agent = agent;
            for (size_t i = 0; i < m_observedKeys.size(); ++i)
            {
                // Seed the seen epoch with the current epoch so a fresh connection does not
                // immediately report dirty for keys that were set earlier.
                m_seenKeyEpochs[i] = m_blackboard->GetKeyEpoch(m_observedKeys[i], m_agent);
            }

            // A freshly connected agent has never evaluated its guards.
            m_forced = true;
            return;
        }

        for (size_t scopeIndex = 0; scopeIndex < ScopeCount; ++scopeIndex)
        {
            if (!program.m_watchedScopes[scopeIndex])
            {
                continue;
            }

            AZ_Warning("GOAT", blackboard.FindStorage(static_cast<BlackboardScope>(scopeIndex), agent) != nullptr,
                "Agent %u guards a variable in a scope it has no storage for, so those guards never fire",
                agent.GetIndex());

            m_watched[scopeIndex] = true;
        }

        m_blackboard = &blackboard;
        m_agent = agent;

        // A freshly connected agent has never evaluated its guards.
        m_forced = true;
    }

    void GuardWatch::Disconnect()
    {
        m_blackboard = nullptr;
        m_agent = AgentId{};
        m_watched.fill(false);
        m_seen.fill(0);
        m_observedKeys.clear();
        m_seenKeyEpochs.clear();
        m_forced = true;
    }

    AZ::u32 GuardWatch::EpochOf(size_t scopeIndex) const
    {
        const BlackboardStorage* storage =
            m_blackboard->FindStorage(static_cast<BlackboardScope>(scopeIndex), m_agent);
        return storage != nullptr ? storage->GetEpoch() : 0;
    }

    bool GuardWatch::IsDirty() const
    {
        if (m_forced)
        {
            return true;
        }

        if (m_blackboard == nullptr)
        {
            return false;
        }

        // If observing keys, check their per-key epoch values.
        if (!m_observedKeys.empty())
        {
            for (size_t i = 0; i < m_observedKeys.size(); ++i)
            {
                if (m_blackboard->GetKeyEpoch(m_observedKeys[i], m_agent) != m_seenKeyEpochs[i])
                {
                    return true;
                }
            }
            return false;
        }

        for (size_t scopeIndex = 0; scopeIndex < ScopeCount; ++scopeIndex)
        {
            if (m_watched[scopeIndex] && EpochOf(scopeIndex) != m_seen[scopeIndex])
            {
                return true;
            }
        }

        return false;
    }

    void GuardWatch::Clear()
    {
        m_forced = false;
        if (m_blackboard == nullptr)
        {
            return;
        }

        if (!m_observedKeys.empty())
        {
            for (size_t i = 0; i < m_observedKeys.size(); ++i)
            {
                m_seenKeyEpochs[i] = m_blackboard->GetKeyEpoch(m_observedKeys[i], m_agent);
            }
            return;
        }

        for (size_t scopeIndex = 0; scopeIndex < ScopeCount; ++scopeIndex)
        {
            if (m_watched[scopeIndex])
            {
                m_seen[scopeIndex] = EpochOf(scopeIndex);
            }
        }
    }
} // namespace GOAT
