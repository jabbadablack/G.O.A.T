#include <UtilityBackend.h>
#include <UtilityCompiler.h>
#include <Core/Application/BlackboardSystem.h>
#include <Core/Application/NodeTypeRegistry.h>
#include <TestAgentSystem.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    namespace
    {
        //! A verb that only has to exist, so a choice has something to run.
        class ScoredVerb final : public IActionState
        {
        public:
            AZ_RTTI(ScoredVerb, "{2C9A61B7-5E30-4D8F-91A4-7B0C3E6D2F58}", IActionState);
            explicit ScoredVerb(const char* name) : m_name(name) {}
            AZ::Name GetName() const override { return m_name; }
            ActionResult Step(const ActionContext&, float) override { return ActionResult::Success; }
            AZ::Name m_name;
        };
    } // namespace

    //! Builds programs by hand, so each test states the exact choices it means.
    class UtilityFixture : public UnitTest::LeakDetectionFixture
    {
    protected:
        void SetUp() override
        {
            UnitTest::LeakDetectionFixture::SetUp();
            AZ::NameDictionary::Create();

            m_blackboard = AZStd::make_unique<BlackboardSystem>();
            m_actions = AZStd::make_unique<ActionStateRegistry>();
            m_actions->Register(AZStd::make_unique<ScoredVerb>("wait"));
            m_actions->Register(AZStd::make_unique<ScoredVerb>("shout"));

            // `wait` is a built-in word; `shout` stands in for one a module contributed.
            m_nodeTypes = AZStd::make_unique<NodeTypeRegistry>();
            m_host = AZStd::make_unique<TestAgentSystem>(*m_nodeTypes, *m_actions);

            NodeTypeDescriptor shout;
            shout.m_name = AZ_NAME_LITERAL("shout");
            shout.m_kind = NodeKind::Leaf;
            shout.m_op = NodeOp::Action;
            m_nodeTypes->Register(AZStd::move(shout));

            m_agent = AgentId(0, 1);
            m_blackboard->CreateAgentBlackboard(m_agent);
        }

        void TearDown() override
        {
            m_host.reset();
            m_nodeTypes.reset();
            m_actions.reset();
            m_blackboard.reset();
            AZ::NameDictionary::Destroy();
            UnitTest::LeakDetectionFixture::TearDown();
        }

        BlackboardKey DeclareFloat(const char* name, float value, BlackboardScope scope = BlackboardScope::Agent)
        {
            const auto declared = m_blackboard->Declare(AZ::Name(name), scope, BlackboardType::Float);
            EXPECT_TRUE(declared.IsSuccess());
            m_blackboard->Set<float>(declared.GetValue(), value, m_agent);
            return declared.GetValue();
        }

        void SetFloat(const char* name, float value)
        {
            EXPECT_TRUE(m_blackboard->Set<float>(m_blackboard->FindKey(AZ::Name(name)), value, m_agent));
        }

        static AuthoredNode Node(const char* type) { AuthoredNode node; node.m_type = type; return node; }

        static void Text(AuthoredNode& node, const char* key, const char* value)
        {
            AuthoredProperty property;
            property.m_name = key;
            property.m_value = AZStd::string(value);
            node.m_properties.push_back(property);
        }

        static void Flag(AuthoredNode& node, const char* key, bool value)
        {
            AuthoredProperty property;
            property.m_name = key;
            property.m_value = value;
            node.m_properties.push_back(property);
        }

        static void Number(AuthoredNode& node, const char* key, double value)
        {
            AuthoredProperty property;
            property.m_name = key;
            property.m_value = value;
            node.m_properties.push_back(property);
        }

        //! `choice "name" { <verb> }`
        static AuthoredNode Choice(const char* name, const char* verb = "wait")
        {
            AuthoredNode choice = Node("choice");
            Text(choice, "name", name);
            choice.m_children.push_back(Node(verb));
            return choice;
        }

        static AuthoredNode Consider(const char* key)
        {
            AuthoredNode node = Node("consider");
            Text(node, "key", key);
            return node;
        }

        //! Puts a consideration before the verb, which is how one is written.
        static void Considers(AuthoredNode& choice, const char* key)
        {
            choice.m_children.insert(choice.m_children.begin(), Consider(key));
        }

        static AuthoredNode Program()
        {
            return Node("utility");
        }

        AZ::Outcome<UtilityProgram, AZStd::string> Compile(const AuthoredNode& root) const
        {
            const UtilityCompiler compiler(*m_host, *m_blackboard);
            return compiler.Compile(AZ::Name("Test"), root);
        }

        AgentId m_agent;
        AZStd::unique_ptr<BlackboardSystem> m_blackboard;
        AZStd::unique_ptr<ActionStateRegistry> m_actions;
        AZStd::unique_ptr<NodeTypeRegistry> m_nodeTypes;
        AZStd::unique_ptr<TestAgentSystem> m_host;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Compiling.

    TEST_F(UtilityFixture, Compile_RefusesAProgramWithNothingToChooseBetween)
    {
        const auto compiled = Compile(Program());
        ASSERT_FALSE(compiled.IsSuccess());
        EXPECT_NE(compiled.GetError().find("nothing to choose between"), AZStd::string::npos)
            << compiled.GetError().c_str();
    }

    TEST_F(UtilityFixture, Compile_RefusesAnUndeclaredConsideration)
    {
        AuthoredNode root = Program();
        AuthoredNode choice = Choice("Flee");
        Considers(choice, "never_declared");
        root.m_children.push_back(choice);

        const auto compiled = Compile(root);
        ASSERT_FALSE(compiled.IsSuccess());
        EXPECT_NE(compiled.GetError().find("not a declared variable"), AZStd::string::npos)
            << compiled.GetError().c_str();
    }

    //! A score is a float already scaled to 0 to 1, so anything else cannot be one whatever it holds.
    TEST_F(UtilityFixture, Compile_RefusesAConsiderationThatIsNotAFloat)
    {
        const auto declared = m_blackboard->Declare(AZ_NAME_LITERAL("afraid"), BlackboardScope::Agent, BlackboardType::Bool);
        ASSERT_TRUE(declared.IsSuccess());

        AuthoredNode root = Program();
        AuthoredNode choice = Choice("Flee");
        Considers(choice, "afraid");
        root.m_children.push_back(choice);

        const auto compiled = Compile(root);
        ASSERT_FALSE(compiled.IsSuccess());
        EXPECT_NE(compiled.GetError().find("scaled to 0 to 1"), AZStd::string::npos) << compiled.GetError().c_str();
    }

    TEST_F(UtilityFixture, Compile_RefusesAChoiceThatDoesNothing)
    {
        DeclareFloat("fear", 1.0f);

        AuthoredNode root = Program();
        AuthoredNode choice = Node("choice");
        Text(choice, "name", "Flee");
        choice.m_children.push_back(Consider("fear"));
        root.m_children.push_back(choice);

        const auto compiled = Compile(root);
        ASSERT_FALSE(compiled.IsSuccess());
        EXPECT_NE(compiled.GetError().find("does nothing"), AZStd::string::npos) << compiled.GetError().c_str();
    }

    //! A choice's steps already run in order, so the shapes that exist to order things belong to
    //! a tree; the message has to say the way out rather than only that this is not it.
    TEST_F(UtilityFixture, Compile_RefusesATreeShapeInsideAChoice)
    {
        NodeTypeDescriptor sequence;
        sequence.m_name = AZ_NAME_LITERAL("sequence");
        sequence.m_kind = NodeKind::Composite;
        sequence.m_op = NodeOp::Sequence;
        m_nodeTypes->Register(AZStd::move(sequence));

        AuthoredNode root = Program();
        AuthoredNode choice = Choice("Flee", "sequence");
        root.m_children.push_back(choice);

        const auto compiled = Compile(root);
        ASSERT_FALSE(compiled.IsSuccess());
        EXPECT_NE(compiled.GetError().find("should embed"), AZStd::string::npos) << compiled.GetError().c_str();
    }

    //! The core reserves `delegate` so a plan cannot re-enter the tree that asked for it, so a
    //! choice naming one has to be told what to reach another paradigm with instead.
    TEST_F(UtilityFixture, Compile_RefusesADelegateAndNamesWhatToUseInstead)
    {
        AuthoredNode root = Program();
        AuthoredNode choice = Node("choice");
        Text(choice, "name", "Fall back");
        AuthoredNode delegated = Node("delegate");
        Text(delegated, "backend", "htn");
        Text(delegated, "goal", "FallBack");
        choice.m_children.push_back(delegated);
        root.m_children.push_back(choice);

        const auto compiled = Compile(root);
        ASSERT_FALSE(compiled.IsSuccess());
        EXPECT_NE(compiled.GetError().find("embed"), AZStd::string::npos) << compiled.GetError().c_str();
    }

    TEST_F(UtilityFixture, Compile_RefusesTwoChoicesOfTheSameName)
    {
        AuthoredNode root = Program();
        root.m_children.push_back(Choice("Flee"));
        root.m_children.push_back(Choice("Flee"));

        const auto compiled = Compile(root);
        ASSERT_FALSE(compiled.IsSuccess());
        EXPECT_NE(compiled.GetError().find("twice"), AZStd::string::npos) << compiled.GetError().c_str();
    }

    TEST_F(UtilityFixture, Compile_RefusesACombineThatIsNeitherARuleNorABehaviour)
    {
        DeclareFloat("fear", 1.0f);

        AuthoredNode root = Program();
        AuthoredNode choice = Choice("Flee");
        Considers(choice, "fear");
        Text(choice, "combine", "multiplu");
        root.m_children.push_back(choice);

        const auto compiled = Compile(root);
        ASSERT_FALSE(compiled.IsSuccess());
        EXPECT_NE(compiled.GetError().find("nor a declared behaviour"), AZStd::string::npos)
            << compiled.GetError().c_str();
    }

    //! Naming a top while picking the best is not a smaller mistake than a bad number; it means
    //! the author expected a draw that is never going to happen.
    TEST_F(UtilityFixture, Compile_RefusesATopWhenOnlyTheBestIsEverInTheRunning)
    {
        AuthoredNode root = Program();
        Number(root, "top", 3.0);
        root.m_children.push_back(Choice("Flee"));

        const auto compiled = Compile(root);
        ASSERT_FALSE(compiled.IsSuccess());
        EXPECT_NE(compiled.GetError().find("picks the best"), AZStd::string::npos) << compiled.GetError().c_str();
    }

    //! What a choice's steps write is what the agent does, not what it decides on, so a program
    //! that watched its own output would wake itself forever.
    TEST_F(UtilityFixture, Compile_WatchesOnlyTheScopesItConsiders)
    {
        DeclareFloat("mood", 1.0f, BlackboardScope::Global);

        AuthoredNode root = Program();
        AuthoredNode choice = Choice("Flee");
        Considers(choice, "mood");
        root.m_children.push_back(choice);

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        EXPECT_TRUE(compiled.GetValue().m_watchedScopes[static_cast<size_t>(BlackboardScope::Global)]);
        EXPECT_FALSE(compiled.GetValue().m_watchedScopes[static_cast<size_t>(BlackboardScope::Agent)]);
        EXPECT_FALSE(compiled.GetValue().m_watchedScopes[static_cast<size_t>(BlackboardScope::Squad)]);
    }

    //! What a step hands work on to belongs to whoever owns that word, which is the core's job.
    TEST_F(UtilityFixture, Compile_RecordsWhatAChoiceHandsToAnotherProgram)
    {
        NodeTypeDescriptor embedding;
        embedding.m_name = AZ_NAME_LITERAL("embed");
        embedding.m_kind = NodeKind::Leaf;
        embedding.m_op = NodeOp::Action;
        embedding.m_nestsProgram = true;
        NodeParameter goal;
        goal.m_name = AZ_NAME_LITERAL("goal");
        goal.m_type = BlackboardType::Name;
        goal.m_required = true;
        embedding.m_parameters.push_back(goal);
        m_nodeTypes->Register(AZStd::move(embedding));
        m_actions->Register(AZStd::make_unique<ScoredVerb>("embed"));

        AuthoredNode root = Program();
        AuthoredNode choice = Node("choice");
        Text(choice, "name", "Assault");
        AuthoredNode embedded = Node("embed");
        Text(embedded, "goal", "ClearRoom");
        choice.m_children.push_back(embedded);
        root.m_children.push_back(choice);

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        ASSERT_EQ(compiled.GetValue().m_nested.size(), 1u);
        EXPECT_EQ(compiled.GetValue().m_nested[0].m_program, AZ_NAME_LITERAL("ClearRoom"));
        EXPECT_TRUE(compiled.GetValue().m_nested[0].m_runsToCompletion);
    }

    //! A choice runs any verb a module contributed, read through the word's own properties
    //! rather than through a list of the few this paradigm happens to know.
    TEST_F(UtilityFixture, Compile_CarriesAModuleWordsPropertiesIntoTheRequest)
    {
        const BlackboardKey where =
            m_blackboard->Declare(AZ_NAME_LITERAL("cover"), BlackboardScope::Agent, BlackboardType::Vector3)
                .GetValue();

        NodeTypeDescriptor moveTo;
        moveTo.m_name = AZ_NAME_LITERAL("move_to");
        moveTo.m_kind = NodeKind::Leaf;
        moveTo.m_op = NodeOp::Action;
        NodeParameter key;
        key.m_name = AZ_NAME_LITERAL("key");
        key.m_type = BlackboardType::Vector3;
        key.m_isBlackboardKey = true;
        key.m_required = true;
        moveTo.m_parameters.push_back(key);
        NodeParameter tolerance;
        tolerance.m_name = AZ_NAME_LITERAL("tolerance");
        tolerance.m_type = BlackboardType::Float;
        moveTo.m_parameters.push_back(tolerance);
        m_nodeTypes->Register(AZStd::move(moveTo));
        m_actions->Register(AZStd::make_unique<ScoredVerb>("move_to"));

        AuthoredNode root = Program();
        AuthoredNode choice = Node("choice");
        Text(choice, "name", "Flee");
        AuthoredNode step = Node("move_to");
        Text(step, "key", "cover");
        Number(step, "tolerance", 1.5);
        choice.m_children.push_back(step);
        root.m_children.push_back(choice);

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        ASSERT_EQ(compiled.GetValue().m_steps.size(), 1u);
        EXPECT_EQ(compiled.GetValue().m_steps[0].m_targetKey, where);
        EXPECT_FLOAT_EQ(compiled.GetValue().m_steps[0].m_tolerance, 1.5f);
    }
} // namespace GOAT
