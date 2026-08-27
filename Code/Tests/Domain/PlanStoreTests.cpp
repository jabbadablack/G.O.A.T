#include <GOAT/Domain/PlanStore.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    //! Gives the tests an allocator and a name dictionary, because ActionRequest carries an
    //! AZ::Name and constructing one without a dictionary asserts.
    class PlanStoreFixture : public UnitTest::LeakDetectionFixture
    {
    protected:
        void SetUp() override
        {
            UnitTest::LeakDetectionFixture::SetUp();
            AZ::NameDictionary::Create();
        }

        void TearDown() override
        {
            AZ::NameDictionary::Destroy();
            UnitTest::LeakDetectionFixture::TearDown();
        }

        //! A run of steps distinguishable from any other, so a span can be checked by content.
        static AZStd::vector<ActionRequest> MakeSteps(AZ::u32 count, float firstAmount)
        {
            AZStd::vector<ActionRequest> steps(count);
            for (AZ::u32 i = 0; i < count; ++i)
            {
                steps[i].m_amount = firstAmount + static_cast<float>(i);
            }
            return steps;
        }
    };

    TEST_F(PlanStoreFixture, Bake_CopiesStepsAndReportsCount)
    {
        PlanStore store;
        const AZStd::vector<ActionRequest> steps = MakeSteps(3, 1.0f);

        const PlanStore::Span span = store.Bake(steps.data(), 3);

        EXPECT_EQ(span.m_count, 3u);
        EXPECT_EQ(span.m_block, InvalidPlanBlock);
        EXPECT_FLOAT_EQ(span.m_steps[0].m_amount, 1.0f);
        EXPECT_FLOAT_EQ(span.m_steps[2].m_amount, 3.0f);
        EXPECT_EQ(store.GetBakedCount(), 3u);
    }

    //! The invariant an agent's plan depends on: a baked span is a pointer into the store, so
    //! baking more must never move what an earlier span already points at.
    TEST_F(PlanStoreFixture, Bake_DoesNotMoveAnEarlierSpan)
    {
        PlanStore store;
        const AZStd::vector<ActionRequest> first = MakeSteps(2, 10.0f);

        const PlanStore::Span early = store.Bake(first.data(), 2);
        const ActionRequest* address = early.m_steps;

        // Enough to force several new chunks past the 256 step chunk size.
        for (int i = 0; i < 400; ++i)
        {
            const AZStd::vector<ActionRequest> more = MakeSteps(4, static_cast<float>(i));
            store.Bake(more.data(), 4);
        }

        EXPECT_EQ(early.m_steps, address);
        EXPECT_FLOAT_EQ(early.m_steps[0].m_amount, 10.0f);
        EXPECT_FLOAT_EQ(early.m_steps[1].m_amount, 11.0f);
    }

    //! A plan longer than a chunk still has to be one contiguous span.
    TEST_F(PlanStoreFixture, Bake_HandlesAPlanLongerThanAChunk)
    {
        PlanStore store;
        const AZStd::vector<ActionRequest> steps = MakeSteps(1000, 0.0f);

        const PlanStore::Span span = store.Bake(steps.data(), 1000);

        ASSERT_EQ(span.m_count, 1000u);
        EXPECT_FLOAT_EQ(span.m_steps[0].m_amount, 0.0f);
        EXPECT_FLOAT_EQ(span.m_steps[999].m_amount, 999.0f);
    }

    TEST_F(PlanStoreFixture, Acquire_BorrowsAndReleaseGivesBack)
    {
        PlanStore store;
        const AZStd::vector<ActionRequest> steps = MakeSteps(5, 1.0f);

        const PlanStore::Span span = store.Acquire(steps.data(), 5);
        EXPECT_NE(span.m_block, InvalidPlanBlock);
        EXPECT_EQ(store.GetBorrowedCount(), 1u);

        store.Release(span);
        EXPECT_EQ(store.GetBorrowedCount(), 0u);
    }

    //! What makes the pool a pool: a released block is handed out again rather than reallocated.
    TEST_F(PlanStoreFixture, Acquire_ReusesAReleasedBlock)
    {
        PlanStore store;
        const AZStd::vector<ActionRequest> steps = MakeSteps(4, 1.0f);

        const PlanStore::Span first = store.Acquire(steps.data(), 4);
        const ActionRequest* address = first.m_steps;
        store.Release(first);

        const PlanStore::Span second = store.Acquire(steps.data(), 4);

        EXPECT_EQ(second.m_steps, address);
        EXPECT_EQ(store.GetBorrowedCount(), 1u);
    }

    //! Release must ignore a baked span rather than corrupting the free list with it.
    TEST_F(PlanStoreFixture, Release_IgnoresABakedSpan)
    {
        PlanStore store;
        const AZStd::vector<ActionRequest> steps = MakeSteps(2, 1.0f);

        const PlanStore::Span baked = store.Bake(steps.data(), 2);
        store.Release(baked);

        EXPECT_EQ(store.GetBorrowedCount(), 0u);
    }

    //! Borrowing many plans at once must give every one its own storage.
    TEST_F(PlanStoreFixture, Acquire_GivesConcurrentPlansDistinctStorage)
    {
        PlanStore store;
        const AZStd::vector<ActionRequest> steps = MakeSteps(3, 7.0f);

        AZStd::vector<PlanStore::Span> spans;
        for (int i = 0; i < 32; ++i)
        {
            spans.push_back(store.Acquire(steps.data(), 3));
        }

        EXPECT_EQ(store.GetBorrowedCount(), 32u);
        for (size_t i = 1; i < spans.size(); ++i)
        {
            EXPECT_NE(spans[i].m_steps, spans[i - 1].m_steps);
        }

        for (const PlanStore::Span& span : spans)
        {
            store.Release(span);
        }
        EXPECT_EQ(store.GetBorrowedCount(), 0u);
    }
} // namespace GOAT
