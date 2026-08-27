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
        AZ_Assert(m_observed.empty(), "Connecting must start from a disconnected observer");

        m_observed = program.m_observedKeys;
        AZ_Assert(AZStd::is_sorted(m_observed.begin(), m_observed.end()),
            "Observed keys must be sorted, because OnChanged looks them up with a binary search");
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
            AZ_Warning("GOAT", storage != nullptr,
                "Agent %u guards a variable in a scope it has no storage for, so those guards never fire",
                agent.GetIndex());
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

        // A freshly connected agent has never evaluated its guards, so it starts dirty.
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
