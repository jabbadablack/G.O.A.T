#include <BehaviorTreeWords.h>
#include <TreeCompiler.h>
#include <Core/Application/BlackboardSystem.h>
#include <Core/Application/NodeTypeRegistry.h>
#include <TestAgentSystem.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    class WaitingVerb final : public IActionState
    {
    public:
        AZ_RTTI(WaitingVerb, "{5B0E9A31-84C6-4D77-91F2-6E03A5B84C1D}", IActionState);
        AZ::Name GetName() const override { return AZ::Name("wait"); }
        ActionResult Step(const ActionContext&, float) override { return ActionResult::Running; }
    };

    //! Stands in for the core's embed verb, which a tree needs registered to compile a leaf
    //! that hands work to another program, but whose behaviour is not what is under test here.
    class EmbeddingVerb final : public IActionState
    {
    public:
        AZ_RTTI(EmbeddingVerb, "{0F3D6C57-19A4-4E88-8B2C-7A5E1D46B930}", IActionState);
        AZ::Name GetName() const override { return AZ::Name("embed"); }
        ActionResult Step(const ActionContext&, float) override { return ActionResult::Running; }
    };

    class TreeCompilerFixture : public UnitTest::LeakDetectionFixture
    {
    protected:
        void SetUp() override
        {
            UnitTest::LeakDetectionFixture::SetUp();
            AZ::NameDictionary::Create();

            m_blackboard = AZStd::make_unique<BlackboardSystem>();
            m_actions = AZStd::make_unique<ActionStateRegistry>();
            m_actions->RegisterAt(CoreActions::Wait, AZStd::make_unique<WaitingVerb>());
            m_actions->RegisterAt(CoreActions::Embed, AZStd::make_unique<EmbeddingVerb>());
            m_nodeTypes = AZStd::make_unique<NodeTypeRegistry>();
            for (NodeTypeDescriptor& word : BehaviorTreeWords())
            {
                m_nodeTypes->Register(AZStd::move(word));
            }
            m_agents = AZStd::make_unique<TestAgentSystem>(*m_nodeTypes, *m_actions);

            m_gate = m_blackboard->Declare(AZ::Name("gate"), BlackboardScope::Agent, BlackboardType::Bool)
                         .GetValue();
        }

        void TearDown() override
        {
            m_agents.reset();
            m_nodeTypes.reset();
            m_actions.reset();
            m_blackboard.reset();
            AZ::NameDictionary::Destroy();
            UnitTest::LeakDetectionFixture::TearDown();
        }

        static AuthoredNode Node(const char* type) { AuthoredNode node; node.m_type = type; return node; }

        static void Text(AuthoredNode& node, const char* key, const char* value)
        {
            AuthoredProperty property;
            property.m_name = key;
            property.m_value = AZStd::string(value);
            node.m_properties.push_back(property);
        }

        //! `sequence { condition "gate" <abort?>, wait }`
        AuthoredNode GuardedSequence(const char* abort) const
        {
            AuthoredNode root = Node("sequence");
            AuthoredNode condition = Node("condition");
            Text(condition, "key", "gate");
            if (abort != nullptr)
            {
                Text(condition, "abort", abort);
            }
            root.m_children.push_back(condition);

            AuthoredNode leaf = Node("wait");
            AuthoredProperty seconds;
            seconds.m_name = "seconds";
            seconds.m_value = 1.0;
            leaf.m_properties.push_back(seconds);
            root.m_children.push_back(leaf);
            return root;
        }

        AZ::Outcome<DecisionProgram, AZStd::string> Compile(const AuthoredNode& root) const
        {
            const TreeCompiler compiler(*m_agents, *m_blackboard);
            return compiler.Compile(AZ::Name("Test"), root);
        }

        BlackboardKey m_gate;
        AZStd::unique_ptr<BlackboardSystem> m_blackboard;
        AZStd::unique_ptr<ActionStateRegistry> m_actions;
        AZStd::unique_ptr<NodeTypeRegistry> m_nodeTypes;
        AZStd::unique_ptr<TestAgentSystem> m_agents;
    };

    //! Writing a condition is saying the branch needs it. Nothing else should be required to
    //! make the agent notice when it stops being true.
    TEST_F(TreeCompilerFixture, Condition_IsWatchedWithNoAbortAuthored)
    {
        const auto compiled = Compile(GuardedSequence(nullptr));
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        EXPECT_EQ(compiled.GetValue().m_observedKeys.size(), 1u);
        EXPECT_EQ(compiled.GetValue().m_guardNodes.size(), 1u);
        EXPECT_TRUE(compiled.GetValue().m_watchedScopes[static_cast<size_t>(BlackboardScope::Agent)]);
        EXPECT_EQ(compiled.GetValue().m_nodes[1].m_abort, AbortMode::Self);
    }

    TEST_F(TreeCompilerFixture, Condition_TakesAnAuthoredModeOverTheDefault)
    {
        const auto compiled = Compile(GuardedSequence("lower_priority"));
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        EXPECT_EQ(compiled.GetValue().m_nodes[1].m_abort, AbortMode::LowerPriority);
        EXPECT_EQ(compiled.GetValue().m_observedKeys.size(), 1u);
    }

    //! The opt-out, for a condition that is a one-off check rather than a standing requirement.
    TEST_F(TreeCompilerFixture, Condition_WatchesNothingWhenAbortIsNone)
    {
        const auto compiled = Compile(GuardedSequence("none"));
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        EXPECT_EQ(compiled.GetValue().m_nodes[1].m_abort, AbortMode::None);
        EXPECT_TRUE(compiled.GetValue().m_observedKeys.empty());
        EXPECT_FALSE(compiled.GetValue().m_watchedScopes[static_cast<size_t>(BlackboardScope::Agent)]);
    }

    //! A typo used to mean the same as writing nothing, which turned observation off silently.
    TEST_F(TreeCompilerFixture, Condition_RefusesAnAbortModeItDoesNotKnow)
    {
        const auto compiled = Compile(GuardedSequence("sef"));
        EXPECT_FALSE(compiled.IsSuccess());
    }

    //! A leaf that hands work to another program records the reference, because the compiler
    //! that resolved it is the only thing that knows and the core is what compiles it.
    TEST_F(TreeCompilerFixture, Compile_RecordsWhatALeafHandsToAnotherProgram)
    {
        AuthoredNode root = Node("sequence");

        AuthoredNode embed = Node("embed");
        Text(embed, "goal", "ClearRoom");
        root.m_children.push_back(embed);

        AuthoredNode delegated = Node("delegate");
        Text(delegated, "backend", "htn");
        Text(delegated, "goal", "SecurePerimeter");
        root.m_children.push_back(delegated);

        const TreeCompiler compiler(*m_agents, *m_blackboard);
        auto compiled = compiler.Compile(AZ::Name("Sentry"), root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        const auto& nested = compiled.GetValue().m_nested;
        ASSERT_EQ(nested.size(), 2u);

        // An embed names only the program; which paradigm owns it is answered by its root word.
        EXPECT_EQ(nested[0].m_program, AZ::Name("ClearRoom"));
        EXPECT_TRUE(nested[0].m_backend.IsEmpty());
        EXPECT_TRUE(nested[0].m_runsToCompletion);

        // A delegate names the backend outright, and takes one plan rather than running it out.
        EXPECT_EQ(nested[1].m_program, AZ::Name("SecurePerimeter"));
        EXPECT_EQ(nested[1].m_backend, AZ::Name("htn"));
        EXPECT_FALSE(nested[1].m_runsToCompletion);
    }
} // namespace GOAT
