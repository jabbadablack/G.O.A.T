#include <Core/Application/DecisionBackendRegistry.h>

#include <GOAT/GOATBackendBus.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    class CountingBackend : public IDecisionBackend
    {
    public:
        AZ_RTTI(CountingBackend, "{7A3E5C91-4D28-4B06-9F71-3C05E28B6A47}", IDecisionBackend);
        AZ_CLASS_ALLOCATOR(CountingBackend, AZ::SystemAllocator);

        explicit CountingBackend(const char* name, AZStd::vector<AZ::Name> nodeTypes = {})
            : m_name(name)
            , m_nodeTypes(AZStd::move(nodeTypes))
        {
        }

        AZ::Name GetName() const override { return m_name; }
        AZStd::vector<AZ::Name> GetNodeTypes() const override { return m_nodeTypes; }
        size_t GetStateSize() const override { return 0; }

        CompileOutcome Compile(const AZ::Name& name, const AuthoredNode&) const override
        {
            auto program = AZStd::shared_ptr<AgentProgram>(aznew AgentProgram());
            program->m_name = name;
            return AZ::Success(AZStd::shared_ptr<const AgentProgram>(AZStd::move(program)));
        }

        bool Decide(const PlanContext&, const AgentProgram&, BrainState, ActionPlan&) override
        {
            ++m_decisions;
            return false;
        }

        void Release(const PlanContext&) override { ++m_releases; }

        AZ::Name m_name;
        AZStd::vector<AZ::Name> m_nodeTypes;
        int m_decisions = 0;
        int m_releases = 0;
    };

    //! Stands the bus up without the system component, so the seam is testable on its own.
    class BackendBusHandler : public GOATBackendRequestBus::Handler
    {
    public:
        BackendBusHandler() { GOATBackendRequestBus::Handler::BusConnect(); }
        ~BackendBusHandler() override { GOATBackendRequestBus::Handler::BusDisconnect(); }

        bool RegisterDecisionBackend(AZStd::unique_ptr<IDecisionBackend>& backend) override
        {
            return m_registry.Register(AZStd::move(backend));
        }

        void UnregisterDecisionBackend(const AZ::Name& name) override { m_registry.Unregister(name); }
        IDecisionBackend* FindDecisionBackend(const AZ::Name& name) const override { return m_registry.Find(name); }
        AZStd::vector<AZ::Name> GetDecisionBackendNames() const override { return m_registry.GetNames(); }

        DecisionBackendRegistry m_registry{ "decision backend" };
    };

    class DecisionBackendFixture : public UnitTest::LeakDetectionFixture
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
    };

    TEST_F(DecisionBackendFixture, Register_FindsABackendByName)
    {
        DecisionBackendRegistry registry("decision backend");
        ASSERT_TRUE(registry.Register(AZStd::make_unique<CountingBackend>("bt")));

        ASSERT_NE(registry.Find(AZ::Name("bt")), nullptr);
        EXPECT_EQ(registry.Find(AZ::Name("bt"))->GetName(), AZ::Name("bt"));
        EXPECT_EQ(registry.Find(AZ::Name("htn")), nullptr);
    }

    //! Two paradigms coexist, which is the whole point of the seam.
    TEST_F(DecisionBackendFixture, Register_KeepsSeveralParadigmsSideBySide)
    {
        DecisionBackendRegistry registry("decision backend");
        ASSERT_TRUE(registry.Register(AZStd::make_unique<CountingBackend>("bt")));
        ASSERT_TRUE(registry.Register(AZStd::make_unique<CountingBackend>("htn")));

        EXPECT_EQ(registry.GetNames().size(), 2u);
        EXPECT_NE(registry.Find(AZ::Name("bt")), registry.Find(AZ::Name("htn")));
    }

    TEST_F(DecisionBackendFixture, Register_RefusesATakenName)
    {
        DecisionBackendRegistry registry("decision backend");
        ASSERT_TRUE(registry.Register(AZStd::make_unique<CountingBackend>("bt")));

        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_FALSE(registry.Register(AZStd::make_unique<CountingBackend>("bt")));
        AZ_TEST_STOP_TRACE_SUPPRESSION_NO_COUNT;
    }

    TEST_F(DecisionBackendFixture, Unregister_TakesTheBackendBackOut)
    {
        DecisionBackendRegistry registry("decision backend");
        ASSERT_TRUE(registry.Register(AZStd::make_unique<CountingBackend>("bt")));

        registry.Unregister(AZ::Name("bt"));
        EXPECT_EQ(registry.Find(AZ::Name("bt")), nullptr);
        EXPECT_TRUE(registry.GetNames().empty());
    }

    TEST_F(DecisionBackendFixture, Compile_AnswersAProgramNamedAfterTheTree)
    {
        CountingBackend backend("bt");
        AuthoredNode root;
        root.m_type = "selector";

        const CompileOutcome compiled = backend.Compile(AZ::Name("Patrol"), root);
        ASSERT_TRUE(compiled.IsSuccess());
        ASSERT_NE(compiled.GetValue(), nullptr);
        EXPECT_EQ(compiled.GetValue()->m_name, AZ::Name("Patrol"));
    }

    TEST_F(DecisionBackendFixture, NodeTypes_ReportWhatTheBackendClaims)
    {
        const CountingBackend backend("htn", { AZ::Name("task"), AZ::Name("method") });
        ASSERT_EQ(backend.GetNodeTypes().size(), 2u);
        EXPECT_EQ(backend.GetNodeTypes()[0], AZ::Name("task"));
    }

    //! Ownership has to survive the bus, which is why the parameter is a reference.
    TEST_F(DecisionBackendFixture, Bus_RegistersAndFindsThroughTheBus)
    {
        BackendBusHandler handler;

        AZStd::unique_ptr<IDecisionBackend> backend = AZStd::make_unique<CountingBackend>("bt");
        bool registered = false;
        GOATBackendRequestBus::BroadcastResult(
            registered, &GOATBackendRequests::RegisterDecisionBackend, backend);

        EXPECT_TRUE(registered);
        EXPECT_EQ(backend, nullptr);

        IDecisionBackend* found = nullptr;
        GOATBackendRequestBus::BroadcastResult(
            found, &GOATBackendRequests::FindDecisionBackend, AZ::Name("bt"));
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->GetName(), AZ::Name("bt"));
    }

    TEST_F(DecisionBackendFixture, Bus_UnregistersThroughTheBus)
    {
        BackendBusHandler handler;

        AZStd::unique_ptr<IDecisionBackend> backend = AZStd::make_unique<CountingBackend>("bt");
        bool registered = false;
        GOATBackendRequestBus::BroadcastResult(
            registered, &GOATBackendRequests::RegisterDecisionBackend, backend);
        ASSERT_TRUE(registered);

        GOATBackendRequestBus::Broadcast(&GOATBackendRequests::UnregisterDecisionBackend, AZ::Name("bt"));

        AZStd::vector<AZ::Name> names;
        GOATBackendRequestBus::BroadcastResult(names, &GOATBackendRequests::GetDecisionBackendNames);
        EXPECT_TRUE(names.empty());
    }
} // namespace GOAT
