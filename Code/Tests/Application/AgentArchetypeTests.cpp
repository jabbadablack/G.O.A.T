#include <Core/Application/AgentArchetype.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    class AgentArchetypeFixture : public UnitTest::LeakDetectionFixture
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

        //! A program with a node in it, so it counts as compiled rather than empty.
        static AZStd::shared_ptr<const DecisionProgram> MakeProgram(const char* name)
        {
            auto program = AZStd::shared_ptr<DecisionProgram>(aznew DecisionProgram());
            program->m_name = AZ::Name(name);
            // Emplaced rather than braced: AZ::EntityId's default constructor is explicit.
            program->m_nodes.emplace_back();
            return program;
        }
    };

    TEST_F(AgentArchetypeFixture, FindTree_AnswersTheSlotATreeWasAddedIn)
    {
        AgentArchetype archetype;
        archetype.Add(AZ::Name("Wander"), MakeProgram("Wander"));
        archetype.Add(AZ::Name("Rally"), MakeProgram("Rally"));

        EXPECT_EQ(archetype.FindTree(AZ::Name("Wander")), 0);
        EXPECT_EQ(archetype.FindTree(AZ::Name("Rally")), 1);
        EXPECT_EQ(archetype.FindTree(AZ::Name("Flee")), InvalidTreeSlot);
    }

    //! The sharing rule. An entity that declared a tree which had not compiled yet still takes a
    //! slot for it, so the archetype describes what was asked for and the next entity authored
    //! the same way matches it. Omitting the name instead made every such entity build its own.
    TEST_F(AgentArchetypeFixture, Matches_HoldsWhenADeclaredTreeHasNotCompiledYet)
    {
        const AZ::Name declared[] = { AZ::Name("Wander"), AZ::Name("Rally") };

        AgentArchetype archetype;
        archetype.Add(declared[0], MakeProgram("Wander"));
        archetype.Add(declared[1], nullptr);

        EXPECT_TRUE(archetype.Matches(declared));
        EXPECT_EQ(archetype.GetProgram(1), nullptr);
    }

    //! And what makes the empty slot temporary rather than permanent.
    TEST_F(AgentArchetypeFixture, Resolve_FillsADeclaredSlotOnceItsTreeCompiles)
    {
        AgentArchetype archetype;
        archetype.Add(AZ::Name("Wander"), MakeProgram("Wander"));
        archetype.Add(AZ::Name("Rally"), nullptr);

        EXPECT_TRUE(archetype.Resolve(AZ::Name("Rally"), MakeProgram("Rally")));
        ASSERT_NE(archetype.GetProgram(1), nullptr);
        EXPECT_EQ(archetype.GetProgram(1)->m_name, AZ::Name("Rally"));
    }

    //! A tree an agent may already be running is never swapped underneath it, and a tree this
    //! archetype never declared is not adopted by one that happens to compile later.
    TEST_F(AgentArchetypeFixture, Resolve_RefusesAFilledSlotAndAnUndeclaredTree)
    {
        AgentArchetype archetype;
        archetype.Add(AZ::Name("Wander"), MakeProgram("Wander"));

        EXPECT_FALSE(archetype.Resolve(AZ::Name("Wander"), MakeProgram("Other")));
        EXPECT_EQ(archetype.GetProgram(0)->m_name, AZ::Name("Wander"));
        EXPECT_FALSE(archetype.Resolve(AZ::Name("Flee"), MakeProgram("Flee")));
    }
} // namespace GOAT
