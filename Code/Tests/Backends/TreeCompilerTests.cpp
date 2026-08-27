#include <Backends/BehaviorTree/BehaviorTreeWords.h>
#include <Backends/BehaviorTree/TreeCompiler.h>
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
} // namespace GOAT
