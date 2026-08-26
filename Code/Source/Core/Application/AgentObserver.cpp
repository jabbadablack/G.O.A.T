#include <Core/Application/AgentObserver.h>

#include <AzCore/std/algorithm.h>

namespace GOAT
{
    void AgentObserver::OnChanged(BlackboardKey key)
    {
        if (AZStd::binary_search(m_observed.begin(), m_observed.end(), key))
        {
            m_dirty = true;
        }
    }

    void AgentObserver::Connect(const DecisionProgram& program, IBlackboardSystem& blackboard, AgentId agent)
    {
        Disconnect();

        m_observed = program.m_observedKeys;
        if (m_observed.empty())
        {
            // A tree with no guards never needs waking.
            return;
        }

        for (AZ::u8 scopeIndex = 0; scopeIndex < static_cast<AZ::u8>(BlackboardScope::Count); ++scopeIndex)
        {
            const auto scope = static_cast<BlackboardScope>(scopeIndex);
            const bool watchesScope = AZStd::any_of(
                m_observed.begin(), m_observed.end(),
                [scope](BlackboardKey key)
                {
                    return key.GetScope() == scope;
                });

            if (!watchesScope)
            {
                continue;
            }

            BlackboardStorage* storage = blackboard.FindStorage(scope, agent);
            if (storage == nullptr)
            {
                continue;
            }

            m_handlers[scopeIndex] = BlackboardStorage::ChangedEvent::Handler(
                [this](BlackboardKey key)
                {
                    OnChanged(key);
                });
            storage->ConnectChangedHandler(m_handlers[scopeIndex]);
        }

        m_dirty = true;
    }

    void AgentObserver::Disconnect()
    {
        for (auto& handler : m_handlers)
        {
            handler.Disconnect();
        }
        m_observed.clear();
    }
} // namespace GOAT
