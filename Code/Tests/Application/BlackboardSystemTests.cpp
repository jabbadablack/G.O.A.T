#include <Core/Application/BlackboardSystem.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    class BlackboardSystemFixture : public UnitTest::LeakDetectionFixture
    {
    protected:
        void SetUp() override
        {
            UnitTest::LeakDetectionFixture::SetUp();
            AZ::NameDictionary::Create();
            m_blackboard = AZStd::make_unique<BlackboardSystem>();
        }

        void TearDown() override
        {
            m_blackboard.reset();
            AZ::NameDictionary::Destroy();
            UnitTest::LeakDetectionFixture::TearDown();
        }

        BlackboardKey Declare(const char* name, BlackboardScope scope)
        {
            const auto declared = m_blackboard->Declare(AZ::Name(name), scope, BlackboardType::Int);
            EXPECT_TRUE(declared.IsSuccess());
            return declared.GetValue();
        }

        AZStd::unique_ptr<BlackboardSystem> m_blackboard;
    };

    //! Agent storage is reached by slot index now, so the generation is the only thing stopping
    //! a departed agent's handle from reading whoever took its slot. This is the test that
    //! matters: a bare index would hand back the newcomer's values.
    TEST_F(BlackboardSystemFixture, FindStorage_RefusesAStaleHandleAfterItsSlotIsReused)
    {
        const BlackboardKey key = Declare("score", BlackboardScope::Agent);

        const AgentId first(4, 1);
        m_blackboard->CreateAgentBlackboard(first);
        EXPECT_TRUE(m_blackboard->Set<AZ::s64>(key, 11, first));

        m_blackboard->DestroyAgentBlackboard(first);

        // The same slot, taken by a later agent.
        const AgentId second(4, 2);
        m_blackboard->CreateAgentBlackboard(second);

        EXPECT_EQ(m_blackboard->FindStorage(BlackboardScope::Agent, first), nullptr);
        EXPECT_EQ(m_blackboard->Find<AZ::s64>(key, first), nullptr);

        ASSERT_NE(m_blackboard->Find<AZ::s64>(key, second), nullptr);
        EXPECT_EQ(*m_blackboard->Find<AZ::s64>(key, second), 0);
    }

    //! Reusing a slot must not inherit the previous occupant's values either.
    TEST_F(BlackboardSystemFixture, CreateAgentBlackboard_StartsAReusedSlotClean)
    {
        const BlackboardKey key = Declare("score", BlackboardScope::Agent);

        const AgentId first(2, 1);
        m_blackboard->CreateAgentBlackboard(first);
        m_blackboard->Set<AZ::s64>(key, 99, first);
        m_blackboard->DestroyAgentBlackboard(first);

        const AgentId second(2, 2);
        m_blackboard->CreateAgentBlackboard(second);

        ASSERT_NE(m_blackboard->Find<AZ::s64>(key, second), nullptr);
        EXPECT_EQ(*m_blackboard->Find<AZ::s64>(key, second), 0);
    }

    //! Agents are reached by slot, so a sparse set of slots must each find their own storage.
    TEST_F(BlackboardSystemFixture, FindStorage_KeepsSparseSlotsApart)
    {
        const BlackboardKey key = Declare("score", BlackboardScope::Agent);

        const AgentId low(0, 1);
        const AgentId high(500, 1);
        m_blackboard->CreateAgentBlackboard(low);
        m_blackboard->CreateAgentBlackboard(high);

        EXPECT_TRUE(m_blackboard->Set<AZ::s64>(key, 1, low));
        EXPECT_TRUE(m_blackboard->Set<AZ::s64>(key, 2, high));

        EXPECT_EQ(*m_blackboard->Find<AZ::s64>(key, low), 1);
        EXPECT_EQ(*m_blackboard->Find<AZ::s64>(key, high), 2);
    }

    //! A variable declared after agents exist has to reach the ones already registered.
    TEST_F(BlackboardSystemFixture, Declare_GrowsStorageThatAlreadyExists)
    {
        const AgentId agent(3, 1);
        m_blackboard->CreateAgentBlackboard(agent);

        const BlackboardKey late = Declare("late", BlackboardScope::Agent);

        EXPECT_TRUE(m_blackboard->Set<AZ::s64>(late, 5, agent));
        ASSERT_NE(m_blackboard->Find<AZ::s64>(late, agent), nullptr);
        EXPECT_EQ(*m_blackboard->Find<AZ::s64>(late, agent), 5);
    }

    //! Only a real change counts, which is what keeps a repeated write from waking anybody.
    TEST_F(BlackboardSystemFixture, Set_CountsOnlyRealChanges)
    {
        const BlackboardKey key = Declare("score", BlackboardScope::Global);
        const BlackboardStorage* storage = m_blackboard->FindStorage(BlackboardScope::Global, AgentId{});
        ASSERT_NE(storage, nullptr);

        const AZ::u32 before = storage->GetEpoch();
        EXPECT_TRUE(m_blackboard->Set<AZ::s64>(key, 3, AgentId{}));
        const AZ::u32 afterChange = storage->GetEpoch();
        EXPECT_NE(afterChange, before);

        EXPECT_TRUE(m_blackboard->Set<AZ::s64>(key, 3, AgentId{}));
        EXPECT_EQ(storage->GetEpoch(), afterChange);
    }
} // namespace GOAT
