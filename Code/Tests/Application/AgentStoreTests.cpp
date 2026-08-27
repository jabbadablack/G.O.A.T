#include <Core/Application/AgentStore.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    class AgentStoreFixture : public UnitTest::LeakDetectionFixture
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

        static AZStd::unique_ptr<AgentRecord> MakeRecord(AZ::u64 entity)
        {
            auto record = AZStd::make_unique<AgentRecord>();
            record->m_entity = AZ::EntityId(entity);
            return record;
        }
    };

    TEST_F(AgentStoreFixture, Acquire_ReturnsAHandleThatFindsTheRecord)
    {
        AgentStore store;
        const AgentId agent = store.Acquire(MakeRecord(7));

        ASSERT_NE(store.Find(agent), nullptr);
        EXPECT_EQ(store.Find(agent)->m_entity, AZ::EntityId(7));
        EXPECT_EQ(store.Size(), 1u);
    }

    TEST_F(AgentStoreFixture, Release_DropsTheRecordAndTheHandle)
    {
        AgentStore store;
        const AgentId agent = store.Acquire(MakeRecord(7));

        EXPECT_TRUE(store.Release(agent));
        EXPECT_EQ(store.Find(agent), nullptr);
        EXPECT_EQ(store.Size(), 0u);
        EXPECT_FALSE(store.Release(agent));
    }

    //! The reason handles carry a generation at all. Registering a new agent reuses the freed
    //! slot, and the old handle now names a slot that holds somebody else -- it must still
    //! resolve to nothing. A design that stored a bare index would hand back the newcomer.
    TEST_F(AgentStoreFixture, Find_RefusesAStaleHandleAfterItsSlotIsReused)
    {
        AgentStore store;
        const AgentId first = store.Acquire(MakeRecord(1));
        ASSERT_TRUE(store.Release(first));

        const AgentId second = store.Acquire(MakeRecord(2));

        // The slot really was reused, or this test proves nothing.
        EXPECT_EQ(first.GetIndex(), second.GetIndex());
        EXPECT_NE(first.GetGeneration(), second.GetGeneration());

        EXPECT_EQ(store.Find(first), nullptr);
        ASSERT_NE(store.Find(second), nullptr);
        EXPECT_EQ(store.Find(second)->m_entity, AZ::EntityId(2));
    }

    //! A slot index has to keep meaning the same thing for its agent's whole life, because
    //! every other per agent table is about to be indexed by it.
    TEST_F(AgentStoreFixture, Acquire_NeverMovesALiveAgentsSlot)
    {
        AgentStore store;
        const AgentId kept = store.Acquire(MakeRecord(100));
        const AZ::u32 slot = kept.GetIndex();

        AZStd::vector<AgentId> others;
        for (AZ::u64 i = 0; i < 64; ++i)
        {
            others.push_back(store.Acquire(MakeRecord(i)));
        }

        // Releasing from the middle must not compact the survivors down over the hole.
        for (size_t i = 0; i < others.size(); i += 2)
        {
            store.Release(others[i]);
        }

        EXPECT_EQ(kept.GetIndex(), slot);
        ASSERT_NE(store.Find(kept), nullptr);
        EXPECT_EQ(store.Find(kept)->m_entity, AZ::EntityId(100));
    }

    //! Churn must reuse holes rather than growing for ever.
    TEST_F(AgentStoreFixture, Acquire_ReusesHolesAcrossHeavyChurn)
    {
        AgentStore store;
        AgentId agent = store.Acquire(MakeRecord(0));
        for (AZ::u64 i = 0; i < 1000; ++i)
        {
            ASSERT_TRUE(store.Release(agent));
            agent = store.Acquire(MakeRecord(i));
        }

        EXPECT_EQ(store.Size(), 1u);
        EXPECT_EQ(store.GetSlotCount(), 1u);
    }

    //! Iteration walks slots, so a hole answers with a null handle rather than ending the walk.
    TEST_F(AgentStoreFixture, GetHandleAt_ReportsHolesAsNull)
    {
        AgentStore store;
        const AgentId first = store.Acquire(MakeRecord(1));
        const AgentId second = store.Acquire(MakeRecord(2));
        ASSERT_TRUE(store.Release(first));

        ASSERT_EQ(store.GetSlotCount(), 2u);
        EXPECT_TRUE(store.GetHandleAt(first.GetIndex()).IsNull());
        EXPECT_EQ(store.GetHandleAt(second.GetIndex()), second);
    }
} // namespace GOAT
