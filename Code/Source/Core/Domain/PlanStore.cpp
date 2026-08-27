#include <GOAT/Domain/PlanStore.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/std/algorithm.h>

namespace GOAT
{
    PlanStore::Span PlanStore::Bake(const ActionRequest* steps, AZ::u32 count)
    {
        AZ_Assert(steps != nullptr, "Baking a plan needs the steps to bake");
        AZ_Assert(count > 0, "An empty plan is a refusal, not something to bake");

        Span span;
        if (steps == nullptr || count == 0)
        {
            return span;
        }

        // Reuse the last chunk while it has room. Earlier chunks are not revisited: a chunk that
        // filled up stays full, which keeps baking linear rather than a search.
        Chunk* chunk = m_baked.empty() ? nullptr : m_baked.back().get();
        if (chunk == nullptr || chunk->m_used + count > chunk->m_steps.size())
        {
            // A plan longer than a chunk gets one sized to fit, so a span is always contiguous.
            const size_t capacity = AZStd::max(BakedChunkSteps, static_cast<size_t>(count));

            auto fresh = AZStd::make_unique<Chunk>();
            fresh->m_steps.resize(capacity);
            m_baked.push_back(AZStd::move(fresh));
            chunk = m_baked.back().get();
        }

        AZ_Assert(chunk->m_used + count <= chunk->m_steps.size(), "A baked plan must fit its chunk");

        ActionRequest* destination = chunk->m_steps.data() + chunk->m_used;
        for (AZ::u32 i = 0; i < count; ++i)
        {
            destination[i] = steps[i];
        }
        chunk->m_used += count;
        m_bakedCount += count;

        span.m_steps = destination;
        span.m_count = count;
        span.m_block = InvalidPlanBlock;

        AZ_Assert(!span.IsEmpty(), "Baking must produce a span an agent can run");
        return span;
    }

    void PlanStore::ClearBaked()
    {
        AZLOG_INFO("GOAT: dropping %zu baked plan step(s)", m_bakedCount);

        m_baked.clear();
        m_bakedCount = 0;
    }

    PlanStore::Span PlanStore::Acquire(const ActionRequest* steps, AZ::u32 count)
    {
        AZ_Assert(steps != nullptr, "Borrowing room for a plan needs the steps to copy in");
        AZ_Assert(count > 0, "An empty plan is a refusal, not something to borrow room for");

        Span span;
        if (steps == nullptr || count == 0)
        {
            return span;
        }

        // A free block only fits if it is already big enough; blocks keep whatever capacity they
        // grew to, so after warm up this is a pop rather than an allocation.
        PlanBlockId chosen = InvalidPlanBlock;
        for (size_t i = m_freeBlocks.size(); i > 0; --i)
        {
            const PlanBlockId candidate = m_freeBlocks[i - 1];
            AZ_Assert(candidate != InvalidPlanBlock, "The free list must not hold the null block id");

            if (m_blocks[candidate - 1]->m_steps.size() >= count)
            {
                chosen = candidate;
                m_freeBlocks.erase(m_freeBlocks.begin() + (i - 1));
                break;
            }
        }

        if (chosen == InvalidPlanBlock)
        {
            auto fresh = AZStd::make_unique<Block>();
            fresh->m_steps.resize(count);
            m_blocks.push_back(AZStd::move(fresh));

            // Ids are one based so that zero can mean "not borrowed".
            chosen = static_cast<PlanBlockId>(m_blocks.size());
        }

        Block& block = *m_blocks[chosen - 1];
        AZ_Assert(!block.m_borrowed, "A block taken from the free list must not already be borrowed");
        AZ_Assert(block.m_steps.size() >= count, "A borrowed block must be large enough for the plan");

        block.m_borrowed = true;
        for (AZ::u32 i = 0; i < count; ++i)
        {
            block.m_steps[i] = steps[i];
        }

        span.m_steps = block.m_steps.data();
        span.m_count = count;
        span.m_block = chosen;

        AZ_Assert(span.m_block != InvalidPlanBlock, "A borrowed span must carry the block to give back");
        return span;
    }

    void PlanStore::Release(Span span)
    {
        // A baked plan carries no block, so releasing one is a no op rather than an error.
        if (span.m_block == InvalidPlanBlock)
        {
            return;
        }

        AZ_Assert(span.m_block <= m_blocks.size(), "Releasing a block that was never borrowed");
        if (span.m_block > m_blocks.size())
        {
            return;
        }

        Block& block = *m_blocks[span.m_block - 1];
        AZ_Assert(block.m_borrowed, "Releasing a plan block twice");
        if (!block.m_borrowed)
        {
            return;
        }

        // The capacity is kept, not freed: reusing it is the point of the pool.
        block.m_borrowed = false;
        m_freeBlocks.push_back(span.m_block);
    }

    size_t PlanStore::GetBorrowedCount() const
    {
        AZ_Assert(m_freeBlocks.size() <= m_blocks.size(), "More plan blocks are free than exist");
        return m_blocks.size() - m_freeBlocks.size();
    }
} // namespace GOAT
