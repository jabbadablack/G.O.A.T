#include <Core/Application/ActionStateRegistry.h>
#include <Core/Application/AgentArchetype.h>
#include <Core/Application/AgentRegistry.h>
#include <Core/Application/AgentRuntime.h>
#include <Core/Application/BackendRegistry.h>
#include <Core/Application/BlackboardSystem.h>
#include <Core/Director/DirectorRegistry.h>
#include <Core/Scripting/AgentScriptContext.h>
#include <Core/Scripting/LuaDispatch.h>
#include <Core/Scripting/LuaNodeScripting.h>

#include <GOAT/Domain/AgentProgram.h>
#include <GOAT/Interfaces/IDecisionBackend.h>
#include <GOAT/Domain/PlanStore.h>

#include <AzCore/Console/LoggerSystemComponent.h>
#include <AzCore/EBus/EventSchedulerSystemComponent.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Time/TimeSystem.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/std/algorithm.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    //! Enough of a backend for an agent to count as having something to run. Nothing here ticks,
    //! so it is never asked to decide.
    class IdleBackend final : public IDecisionBackend
    {
    public:
        AZ_RTTI(IdleBackend, "{9C4E71A3-0B58-4D26-8F19-A7C3E05B62D4}", IDecisionBackend);

        AZ::Name GetName() const override { return AZ::Name("idle"); }
        AZStd::vector<AZ::Name> GetNodeTypes() const override { return {}; }
        size_t GetStateSize() const override { return 0; }

        CompileOutcome Compile(const AZ::Name&, const AuthoredNode&) override
        {
            return AZ::Failure(AZStd::string("the idle backend compiles nothing"));
        }

        Decision Decide(const PlanContext&, const AgentProgram&, BrainState, ActionResult, float, ActionPlan&) override
        {
            return Decision{};
        }
    };

    //! Accepts or rejects by a fixed list, so a test says who is governed without needing an
    //! entity, a transform or a shape.
    class ListFilter final : public IDirectorFilter
    {
    public:
        AZ_RTTI(ListFilter, "{3E1D9F60-27B4-4C85-A0F3-98D5B1C74E62}", IDirectorFilter);

        explicit ListFilter(AZStd::vector<AgentId> accepted)
            : m_accepted(AZStd::move(accepted))
        {
        }

        bool Accepts(AgentId agent, AZ::EntityId) const override
        {
            ++m_asked;
            return AZStd::find(m_accepted.begin(), m_accepted.end(), agent) != m_accepted.end();
        }

        AZStd::vector<AgentId> m_accepted;
        mutable int m_asked = 0;
    };

    //! Accepts everyone, which is what a filter does when it cannot answer.
    class OpenFilter final : public IDirectorFilter
    {
    public:
        AZ_RTTI(OpenFilter, "{B4A7C218-5D39-4E06-91F7-2C68A0D53B14}", IDirectorFilter);

        bool Accepts(AgentId, AZ::EntityId) const override { return true; }
    };

    class DirectorFilterFixture : public UnitTest::LeakDetectionFixture
    {
    protected:
        void SetUp() override
        {
            UnitTest::LeakDetectionFixture::SetUp();
            AZ::NameDictionary::Create();

            // AgentRegistry schedules an event per band the moment it is built, so the clock and
            // the scheduler those events live on have to be standing first.
            m_logger = AZStd::make_unique<AZ::LoggerSystemComponent>();
            m_time = AZStd::make_unique<AZ::TimeSystem>();
            m_scheduler = AZStd::make_unique<AZ::EventSchedulerSystemComponent>();

            m_blackboard = AZStd::make_unique<BlackboardSystem>();
            m_actions = AZStd::make_unique<ActionStateRegistry>();
            m_backends = AZStd::make_unique<BackendRegistry>("backend");
            m_dispatch = AZStd::make_unique<LuaDispatch>();
            m_scriptContext = AZStd::make_unique<AgentScriptContext>();
            m_scripting = AZStd::make_unique<LuaNodeScripting>(*m_dispatch, *m_scriptContext);
            m_runtime = AZStd::make_unique<AgentRuntime>(
                *m_blackboard, *m_actions, *m_backends, *m_scripting, m_planStore);
            m_agents = AZStd::make_unique<AgentRegistry>(*m_runtime, *m_blackboard, *m_dispatch);
            m_directors = AZStd::make_unique<DirectorRegistry>(*m_agents);

            // Registering refuses an agent with nothing to run, and these tests never tick, so
            // the emptiest program that counts as compiled is enough.
            auto program = AZStd::shared_ptr<AgentProgram>(aznew AgentProgram());
            program->m_name = AZ::Name("Idle");
            program->m_backend = &m_backend;

            auto archetype = AZStd::shared_ptr<AgentArchetype>(aznew AgentArchetype());
            archetype->Add(AZ::Name("Idle"), AZStd::move(program));
            m_archetype = archetype;
        }

        void TearDown() override
        {
            m_directors.reset();
            m_agents.reset();
            m_runtime.reset();
            m_scripting.reset();
            m_scriptContext.reset();
            m_dispatch.reset();
            m_backends.reset();
            m_actions.reset();
            m_blackboard.reset();
            m_archetype.reset();
            m_scheduler.reset();
            m_time.reset();
            m_logger.reset();
            AZ::NameDictionary::Destroy();
            UnitTest::LeakDetectionFixture::TearDown();
        }

        AgentId AddAgent(AZ::u64 entity)
        {
            return m_agents->Register(AZ::EntityId(entity), m_archetype, 0, AZ::Name{});
        }

        AZStd::unique_ptr<AZ::LoggerSystemComponent> m_logger;
        AZStd::unique_ptr<AZ::TimeSystem> m_time;
        AZStd::unique_ptr<AZ::EventSchedulerSystemComponent> m_scheduler;
        AZStd::unique_ptr<BlackboardSystem> m_blackboard;
        AZStd::unique_ptr<ActionStateRegistry> m_actions;
        AZStd::unique_ptr<BackendRegistry> m_backends;
        AZStd::unique_ptr<LuaDispatch> m_dispatch;
        AZStd::unique_ptr<AgentScriptContext> m_scriptContext;
        AZStd::unique_ptr<LuaNodeScripting> m_scripting;
        PlanStore m_planStore;
        IdleBackend m_backend;
        AZStd::unique_ptr<AgentRuntime> m_runtime;
        AZStd::unique_ptr<AgentRegistry> m_agents;
        AZStd::unique_ptr<DirectorRegistry> m_directors;
        AZStd::shared_ptr<const AgentArchetype> m_archetype;
    };

    //! The whole point of the change: nothing attached means nothing narrowed.
    TEST_F(DirectorFilterFixture, Resolve_GovernsEveryAgentButItselfWithNoFilter)
    {
        const AgentId director = AddAgent(1);
        const AgentId first = AddAgent(2);
        const AgentId second = AddAgent(3);
        ASSERT_TRUE(m_directors->Register(director, DirectorProfile{}));

        const AZStd::vector<AgentId>& reach = m_directors->Resolve(director);
        EXPECT_EQ(reach.size(), 2u);
        EXPECT_NE(AZStd::find(reach.begin(), reach.end(), first), reach.end());
        EXPECT_NE(AZStd::find(reach.begin(), reach.end(), second), reach.end());
        EXPECT_EQ(AZStd::find(reach.begin(), reach.end(), director), reach.end());
    }

    TEST_F(DirectorFilterFixture, AttachFilter_NarrowsTheReach)
    {
        const AgentId director = AddAgent(1);
        const AgentId wanted = AddAgent(2);
        AddAgent(3);
        ASSERT_TRUE(m_directors->Register(director, DirectorProfile{}));

        ListFilter filter({ wanted });
        ASSERT_TRUE(m_directors->AttachFilter(director, filter));

        const AZStd::vector<AgentId>& reach = m_directors->Resolve(director);
        ASSERT_EQ(reach.size(), 1u);
        EXPECT_EQ(reach[0], wanted);
    }

    //! Two filters are an AND, which is what lets an area and a squad be authored together.
    TEST_F(DirectorFilterFixture, AttachFilter_CombinesSeveralWithAnd)
    {
        const AgentId director = AddAgent(1);
        const AgentId both = AddAgent(2);
        const AgentId onlyFirst = AddAgent(3);
        const AgentId onlySecond = AddAgent(4);
        ASSERT_TRUE(m_directors->Register(director, DirectorProfile{}));

        ListFilter first({ both, onlyFirst });
        ListFilter second({ both, onlySecond });
        ASSERT_TRUE(m_directors->AttachFilter(director, first));
        ASSERT_TRUE(m_directors->AttachFilter(director, second));

        const AZStd::vector<AgentId>& reach = m_directors->Resolve(director);
        ASSERT_EQ(reach.size(), 1u);
        EXPECT_EQ(reach[0], both);
    }

    //! A filter that cannot answer accepts, so a broken setup never quietly shrinks a reach.
    TEST_F(DirectorFilterFixture, AttachFilter_LeavesTheReachWholeWhenItFailsOpen)
    {
        const AgentId director = AddAgent(1);
        AddAgent(2);
        AddAgent(3);
        ASSERT_TRUE(m_directors->Register(director, DirectorProfile{}));

        OpenFilter filter;
        ASSERT_TRUE(m_directors->AttachFilter(director, filter));

        EXPECT_EQ(m_directors->Resolve(director).size(), 2u);
    }

    //! The reach is cached for a whole band, so attaching has to throw that cache away or a
    //! filter added mid band would not apply until the next tick.
    TEST_F(DirectorFilterFixture, AttachFilter_AppliesToTheVeryNextResolve)
    {
        const AgentId director = AddAgent(1);
        const AgentId wanted = AddAgent(2);
        AddAgent(3);
        ASSERT_TRUE(m_directors->Register(director, DirectorProfile{}));
        ASSERT_EQ(m_directors->Resolve(director).size(), 2u);

        ListFilter filter({ wanted });
        ASSERT_TRUE(m_directors->AttachFilter(director, filter));

        EXPECT_EQ(m_directors->Resolve(director).size(), 1u);
    }

    TEST_F(DirectorFilterFixture, DetachFilter_GivesTheReachBack)
    {
        const AgentId director = AddAgent(1);
        const AgentId wanted = AddAgent(2);
        AddAgent(3);
        ASSERT_TRUE(m_directors->Register(director, DirectorProfile{}));

        ListFilter filter({ wanted });
        ASSERT_TRUE(m_directors->AttachFilter(director, filter));
        ASSERT_EQ(m_directors->Resolve(director).size(), 1u);

        m_directors->DetachFilter(director, filter);
        EXPECT_EQ(m_directors->Resolve(director).size(), 2u);
        EXPECT_TRUE(m_directors->GetFilters(director).empty());
    }

    //! A detached filter must stop being asked, or a component that has gone would still vote.
    TEST_F(DirectorFilterFixture, DetachFilter_StopsAskingIt)
    {
        const AgentId director = AddAgent(1);
        AddAgent(2);
        ASSERT_TRUE(m_directors->Register(director, DirectorProfile{}));

        ListFilter filter({});
        ASSERT_TRUE(m_directors->AttachFilter(director, filter));
        m_directors->Resolve(director);
        const int asked = filter.m_asked;
        EXPECT_GT(asked, 0);

        m_directors->DetachFilter(director, filter);
        m_directors->Resolve(director);
        EXPECT_EQ(filter.m_asked, asked);
    }

    TEST_F(DirectorFilterFixture, AttachFilter_RefusesTheSameFilterTwice)
    {
        const AgentId director = AddAgent(1);
        ASSERT_TRUE(m_directors->Register(director, DirectorProfile{}));

        ListFilter filter({});
        ASSERT_TRUE(m_directors->AttachFilter(director, filter));
        EXPECT_FALSE(m_directors->AttachFilter(director, filter));
        EXPECT_EQ(m_directors->GetFilters(director).size(), 1u);
    }

    TEST_F(DirectorFilterFixture, AttachFilter_RefusesAnAgentThatIsNotADirector)
    {
        const AgentId agent = AddAgent(1);

        ListFilter filter({});
        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(m_directors->AttachFilter(agent, filter));
        AZ_TEST_STOP_TRACE_SUPPRESSION_NO_COUNT;
    }
} // namespace GOAT
