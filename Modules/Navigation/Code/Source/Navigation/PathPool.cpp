#include <Navigation/PathPool.h>

#include <AzCore/Debug/Trace.h>

namespace GOAT_Navigation
{
    PathSlot PathPool::Acquire()
    {
        if (!m_free.empty())
        {
            const PathSlot slot = m_free.back();
            m_free.pop_back();

            AZ_Assert(slot < m_entries.size(), "A pooled path slot must index the entry table");
            AZ_Assert(!m_entries[slot].m_borrowed, "A free path slot must not already be borrowed");

            m_entries[slot].m_borrowed = true;
            m_entries[slot].m_path.clear();
            return slot;
        }

        m_entries.push_back(Entry{ {}, true });
        return static_cast<PathSlot>(m_entries.size() - 1);
    }

    void PathPool::Release(PathSlot slot)
    {
        if (slot == InvalidPathSlot)
        {
            return;
        }

        AZ_Assert(slot < m_entries.size(), "Releasing a path slot that was never borrowed");
        if (slot >= m_entries.size())
        {
            return;
        }

        AZ_Assert(m_entries[slot].m_borrowed, "Releasing a path slot twice");
        if (!m_entries[slot].m_borrowed)
        {
            return;
        }

        // The buffer is kept, not freed: reusing its capacity is the point of the pool.
        m_entries[slot].m_path.clear();
        m_entries[slot].m_borrowed = false;
        m_free.push_back(slot);
    }

    AZStd::vector<AZ::Vector3>* PathPool::Find(PathSlot slot)
    {
        if (slot >= m_entries.size() || !m_entries[slot].m_borrowed)
        {
            return nullptr;
        }
        return &m_entries[slot].m_path;
    }

    size_t PathPool::GetBorrowedCount() const
    {
        AZ_Assert(m_free.size() <= m_entries.size(), "More path slots are free than exist");
        return m_entries.size() - m_free.size();
    }
} // namespace GOAT_Navigation
