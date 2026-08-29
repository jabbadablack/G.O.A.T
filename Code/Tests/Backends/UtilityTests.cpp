#include <UtilityBackend.h>
#include <UtilityCompiler.h>
#include <Core/Application/BlackboardSystem.h>
#include <Core/Application/NodeTypeRegistry.h>
#include <GOAT/Domain/PlanStore.h>
#include <TestAgentSystem.h>

#include <AzCore/Debug/TraceMessageBus.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    namespace
    {
        //! Counts the warnings a stretch of code produced. The trace suppression a test usually
        //! reaches for counts asserts and errors, and saying something once is a warning.
        class WarningCounter final
            : public AZ::Debug::TraceMessageBus::Handler
        {
        public:
            WarningCounter() { BusConnect(); }
            ~WarningCounter() override { BusDisconnect(); }

            bool OnPreWarning(const char*, const char*, int, const char*, const char*) override
            {
                ++m_count;
                return true;
            }

            int m_count = 0;
        };

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

    //! What a tool watching an agent is told: which authored choice it settled on.
    TEST_F(UtilityFixture, TheChosenOptionIsReportedByWhereItWasAuthored)
    {
        AuthoredNode root = Program();
        root.m_children.push_back(Choice("Flee"));
        root.m_children.push_back(Choice("Fight"));

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        const UtilityProgram& program = compiled.GetValue();
        ASSERT_EQ(program.m_authored.size(), program.m_choices.size());
        ASSERT_EQ(program.m_choices.size(), 2u);
        ASSERT_EQ(program.m_authored[1].m_path.size(), 1u);
        EXPECT_EQ(program.m_authored[1].m_path[0], 1u);
        EXPECT_EQ(program.m_authored[1].m_program, AZ::Name("Test"));

        UtilityBackend backend(*m_host, *m_blackboard);

        UtilityCursor cursor;
        cursor.m_choice = 1;
        AZStd::vector<ProgramNodeRef> path;
        backend.DescribePosition(program, BrainState(reinterpret_cast<AZ::u8*>(&cursor), sizeof(cursor)),
            NoRunningStep, path);
        ASSERT_EQ(path.size(), 1u) << "a utility program is one level deep";
        EXPECT_EQ(path[0].m_path, program.m_authored[1].m_path);

        cursor.m_choice = InvalidChoice;
        path.clear();
        backend.DescribePosition(program, BrainState(reinterpret_cast<AZ::u8*>(&cursor), sizeof(cursor)),
            NoRunningStep, path);
        EXPECT_TRUE(path.empty()) << "an agent that has chosen nothing is running nothing";
    }

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

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Scoring and running.

    //! Runs a compiled program against a real plan store, the way the runtime would.
    class UtilityRunningFixture : public UtilityFixture
    {
    protected:
        void TearDown() override
        {
            // Given back before the store goes, so what a step holds outlives nothing.
            m_store.Release(m_plan.m_span);
            m_plan = ActionPlan{};
            UtilityFixture::TearDown();
        }

        PlanContext Context(AgentId agent) 
        {
            PlanContext context;
            context.m_agent = agent;
            context.m_blackboard = m_blackboard.get();
            context.m_planStore = &m_store;
            return context;
        }

        //! A brain block for one agent, sized as the archetype would size it.
        struct Brain final
        {
            alignas(8) AZStd::array<AZ::u8, 64> m_bytes{};
            BrainState State() { return BrainState(m_bytes.data(), m_bytes.size()); }
        };

        Decision Decide(UtilityBackend& backend, const UtilityProgram& program, Brain& brain,
            ActionResult last = ActionResult::Success)
        {
            m_store.Release(m_plan.m_span);
            m_plan = ActionPlan{};
            return backend.Decide(Context(m_agent), program, brain.State(), last, 0.0f, m_plan);
        }

        //! The name of the choice an agent settled on, or empty when it settled on none.
        AZ::Name Chose(UtilityBackend& backend, const UtilityProgram& program, Brain& brain)
        {
            const Decision decision = Decide(backend, program, brain);
            if (!decision.m_planned)
            {
                return {};
            }
            const auto* cursor = reinterpret_cast<const UtilityCursor*>(brain.m_bytes.data());
            return program.m_choices[cursor->m_choice].m_name;
        }

        UtilityProgram Built(const AuthoredNode& root)
        {
            auto compiled = Compile(root);
            EXPECT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();
            return compiled.IsSuccess() ? AZStd::move(compiled.GetValue()) : UtilityProgram{};
        }

        PlanStore m_store;
        ActionPlan m_plan;
    };

    //! A consideration is used as it stands: there is no curve, and adding one is the whole
    //! reason a scorer can be written in Lua instead.
    TEST_F(UtilityRunningFixture, Score_RunsTheBestScoringChoice)
    {
        DeclareFloat("fear", 0.2f);
        DeclareFloat("morale", 0.9f);

        AuthoredNode root = Program();
        AuthoredNode flee = Choice("Flee");
        Considers(flee, "fear");
        AuthoredNode fight = Choice("Fight", "shout");
        Considers(fight, "morale");
        root.m_children.push_back(flee);
        root.m_children.push_back(fight);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);
        Brain brain;
        backend.Attach(Context(m_agent), program, brain.State());

        EXPECT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Fight"));

        SetFloat("morale", 0.1f);
        EXPECT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Flee"));
    }

    //! Every step of the winner, not just its first: a choice is a plan.
    TEST_F(UtilityRunningFixture, Decide_ProducesEveryStepOfTheChoiceItPicked)
    {
        AuthoredNode root = Program();
        AuthoredNode choice = Node("choice");
        Text(choice, "name", "Shout twice");
        choice.m_children.push_back(Node("shout"));
        choice.m_children.push_back(Node("shout"));
        choice.m_children.push_back(Node("wait"));
        root.m_children.push_back(choice);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);
        Brain brain;
        backend.Attach(Context(m_agent), program, brain.State());

        const Decision decision = Decide(backend, program, brain);
        ASSERT_TRUE(decision.m_planned);
        EXPECT_EQ(m_plan.Size(), 3u);
    }

    //! Multiplying is what lets one consideration rule a choice out entirely, which is the
    //! whole reason it is the rule a choice gets when it names none.
    TEST_F(UtilityRunningFixture, Score_VetoesAChoiceWhenAnythingItMultipliesIsZero)
    {
        DeclareFloat("ammo", 0.0f);
        DeclareFloat("morale", 1.0f);
        DeclareFloat("calm", 0.1f);

        AuthoredNode root = Program();
        AuthoredNode fight = Choice("Fight", "shout");
        Considers(fight, "morale");
        Considers(fight, "ammo");
        AuthoredNode idle = Choice("Idle");
        Considers(idle, "calm");
        root.m_children.push_back(fight);
        root.m_children.push_back(idle);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);
        Brain brain;
        backend.Attach(Context(m_agent), program, brain.State());

        EXPECT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Idle"));
    }

    //! The same numbers under a different rule are a different answer, which is the point of
    //! letting an author name one.
    TEST_F(UtilityRunningFixture, Score_MeanAndMinFoldTheSameValuesDifferently)
    {
        DeclareFloat("low", 0.2f);
        DeclareFloat("high", 1.0f);
        DeclareFloat("flat", 0.5f);

        auto scored = [&](const char* rule)
        {
            AuthoredNode root = Program();
            AuthoredNode mixed = Choice("Mixed", "shout");
            Considers(mixed, "high");
            Considers(mixed, "low");
            Text(mixed, "combine", rule);
            AuthoredNode flat = Choice("Flat");
            Considers(flat, "flat");
            root.m_children.push_back(mixed);
            root.m_children.push_back(flat);

            const UtilityProgram program = Built(root);
            UtilityBackend backend(*m_host, *m_blackboard);
            Brain brain;
            backend.Attach(Context(m_agent), program, brain.State());
            return Chose(backend, program, brain);
        };

        // mean(1.0, 0.2) is 0.6, which beats a flat 0.5; min is 0.2, which does not.
        EXPECT_EQ(scored("mean"), AZ_NAME_LITERAL("Mixed"));
        EXPECT_EQ(scored("min"), AZ_NAME_LITERAL("Flat"));
    }

    //! A value outside the range is clamped so the agent keeps running, and reported once: the
    //! program is what was written wrongly, and a thousand agents run the same one.
    TEST_F(UtilityRunningFixture, Score_ClampsAValueOutsideTheRangeAndSaysSoOnce)
    {
        DeclareFloat("fear", 7.0f);

        AuthoredNode root = Program();
        AuthoredNode flee = Choice("Flee");
        Considers(flee, "fear");
        root.m_children.push_back(flee);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);
        Brain brain;
        backend.Attach(Context(m_agent), program, brain.State());

        WarningCounter warnings;

        // Clamped rather than refused, so the agent keeps running on a value it can use.
        EXPECT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Flee"));
        EXPECT_EQ(warnings.m_count, 1);

        // Said once, however many times it is scored afterwards: a thousand agents run this
        // same program, and the thing written wrongly is the program.
        EXPECT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Flee"));
        EXPECT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Flee"));
        EXPECT_EQ(warnings.m_count, 1);
    }

    //! Considerations are read and folded before anything else, so a choice nothing argues for
    //! costs no call into a script at all. That ordering is the whole cost argument for scorers.
    TEST_F(UtilityRunningFixture, Score_NeverAsksAScriptAboutAVetoedChoice)
    {
        DeclareFloat("ammo", 0.0f);
        m_host->m_measured[AZ_NAME_LITERAL("Exposure")] = 1.0f;

        AuthoredNode root = Program();
        AuthoredNode snipe = Choice("Snipe", "shout");
        Considers(snipe, "ammo");
        Text(snipe, "score", "Exposure");
        root.m_children.push_back(snipe);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);
        Brain brain;
        backend.Attach(Context(m_agent), program, brain.State());

        EXPECT_TRUE(Chose(backend, program, brain).IsEmpty());
        EXPECT_EQ(m_host->m_measureCalls, 0);

        // With something arguing for it, the scorer is reached.
        SetFloat("ammo", 1.0f);
        EXPECT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Snipe"));
        EXPECT_EQ(m_host->m_measureCalls, 1);
    }

    //! A combining behaviour is handed what the choice considered, in the order it was written,
    //! because that is what an author writes their own maths against.
    TEST_F(UtilityRunningFixture, Score_HandsTheConsideredValuesToTheCombiningBehaviour)
    {
        DeclareFloat("near", 0.25f);
        DeclareFloat("hurt", 0.75f);
        m_host->m_measured[AZ_NAME_LITERAL("SniperMath")] = 0.5f;

        AuthoredNode root = Program();
        AuthoredNode snipe = Node("choice");
        Text(snipe, "name", "Snipe");
        Text(snipe, "combine", "SniperMath");
        snipe.m_children.push_back(Consider("near"));
        snipe.m_children.push_back(Consider("hurt"));
        snipe.m_children.push_back(Node("shout"));
        root.m_children.push_back(snipe);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);
        Brain brain;
        backend.Attach(Context(m_agent), program, brain.State());

        EXPECT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Snipe"));
        ASSERT_EQ(m_host->m_lastMeasured.size(), 2u);
        EXPECT_FLOAT_EQ(m_host->m_lastMeasured[0], 0.25f);
        EXPECT_FLOAT_EQ(m_host->m_lastMeasured[1], 0.75f);
    }

    //! Nothing scores, but what it reads can still move, so it waits rather than giving up.
    TEST_F(UtilityRunningFixture, Decide_WaitsItsRecheckWhenNothingScores)
    {
        DeclareFloat("fear", 0.0f);

        AuthoredNode root = Program();
        Number(root, "recheck", 0.5);
        AuthoredNode flee = Choice("Flee");
        Considers(flee, "fear");
        root.m_children.push_back(flee);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);
        Brain brain;
        backend.Attach(Context(m_agent), program, brain.State());

        const Decision decision = Decide(backend, program, brain);
        EXPECT_FALSE(decision.m_planned);
        EXPECT_FLOAT_EQ(decision.m_wakeIn, 0.5f);

        // Idle, never finished: a choice argues from numbers that move, so asking again later
        // can answer differently. A program that called itself finished here would end the step
        // it was embedded in the first time a threat happened to be out of sight.
        EXPECT_EQ(decision.m_result, ActionResult::Running);

        // And it says so again rather than settling into it.
        SetFloat("fear", 0.9f);
        EXPECT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Flee"));
    }

    //! The adapter behind a `delegate` attaches, asks once and releases, with no Advance and no
    //! elapsed time. A Decide that leant on either would answer nothing here.
    TEST_F(UtilityRunningFixture, Decide_AnswersWhenAttachAndReleaseHappenAroundOneCall)
    {
        DeclareFloat("morale", 0.8f);

        AuthoredNode root = Program();
        AuthoredNode fight = Choice("Fight", "shout");
        Considers(fight, "morale");
        root.m_children.push_back(fight);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);

        Brain brain;
        const PlanContext context = Context(m_agent);
        backend.Attach(context, program, brain.State());
        const Decision decision = backend.Decide(context, program, brain.State(), ActionResult::Success, 0.0f, m_plan);
        backend.Release(context, brain.State());

        EXPECT_TRUE(decision.m_planned);
        EXPECT_EQ(m_plan.Size(), 1u);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Holding on to a choice, and leaving it.

    TEST_F(UtilityRunningFixture, Advance_HoldsTheChoiceUntilTheRecheckFloorHasPassed)
    {
        DeclareFloat("fear", 0.2f);
        DeclareFloat("morale", 0.1f);

        AuthoredNode root = Program();
        Number(root, "recheck", 1.0);
        AuthoredNode flee = Choice("Flee");
        Considers(flee, "fear");
        AuthoredNode fight = Choice("Fight", "shout");
        Considers(fight, "morale");
        root.m_children.push_back(flee);
        root.m_children.push_back(fight);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);
        Brain brain;
        backend.Attach(Context(m_agent), program, brain.State());
        ASSERT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Flee"));

        // Fight now scores higher, but the floor has not passed.
        SetFloat("morale", 0.9f);
        EXPECT_EQ(backend.Advance(Context(m_agent), program, brain.State(), 0.4f, 0), TickResult::Continue);
        EXPECT_EQ(backend.Advance(Context(m_agent), program, brain.State(), 0.4f, 0), TickResult::Continue);

        // Past it, and it is overtaken.
        EXPECT_EQ(backend.Advance(Context(m_agent), program, brain.State(), 0.4f, 0), TickResult::Abandon);
    }

    //! Committing means a better idea arriving halfway is not a reason to drop what is underway.
    TEST_F(UtilityRunningFixture, Advance_KeepsACommittedChoiceWhateverElseScores)
    {
        DeclareFloat("ammo_low", 0.5f);
        DeclareFloat("morale", 0.1f);

        AuthoredNode root = Program();
        Number(root, "recheck", 0.0);
        AuthoredNode reload = Choice("Reload", "shout");
        Considers(reload, "ammo_low");
        Flag(reload, "commit", true);
        AuthoredNode fight = Choice("Fight");
        Considers(fight, "morale");
        root.m_children.push_back(reload);
        root.m_children.push_back(fight);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);
        Brain brain;
        backend.Attach(Context(m_agent), program, brain.State());
        ASSERT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Reload"));

        SetFloat("morale", 1.0f);
        EXPECT_EQ(backend.Advance(Context(m_agent), program, brain.State(), 1.0f, 0), TickResult::Continue);
    }

    TEST_F(UtilityRunningFixture, Advance_AbandonsWhenTheRunningChoiceStopsScoring)
    {
        DeclareFloat("fear", 0.9f);

        AuthoredNode root = Program();
        Number(root, "recheck", 0.0);
        AuthoredNode flee = Choice("Flee");
        Considers(flee, "fear");
        root.m_children.push_back(flee);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);
        Brain brain;
        backend.Attach(Context(m_agent), program, brain.State());
        ASSERT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Flee"));

        SetFloat("fear", 0.0f);
        EXPECT_EQ(backend.Advance(Context(m_agent), program, brain.State(), 1.0f, 0), TickResult::Abandon);
    }

    //! Without momentum two choices this close trade the agent back and forth; with it, the one
    //! already underway keeps it.
    TEST_F(UtilityRunningFixture, Advance_KeepsTheRunningChoiceWhenMomentumCoversTheGap)
    {
        DeclareFloat("fear", 0.50f);
        DeclareFloat("morale", 0.55f);

        auto overtaken = [&](double momentum)
        {
            AuthoredNode root = Program();
            Number(root, "recheck", 0.0);
            Number(root, "momentum", momentum);
            AuthoredNode flee = Choice("Flee");
            Considers(flee, "fear");
            AuthoredNode fight = Choice("Fight", "shout");
            Considers(fight, "morale");
            root.m_children.push_back(flee);
            root.m_children.push_back(fight);

            const UtilityProgram program = Built(root);
            UtilityBackend backend(*m_host, *m_blackboard);
            Brain brain;
            backend.Attach(Context(m_agent), program, brain.State());

            // Put it on Flee, which is the lower of the two.
            SetFloat("morale", 0.0f);
            EXPECT_EQ(Chose(backend, program, brain), AZ_NAME_LITERAL("Flee"));
            SetFloat("morale", 0.55f);

            return backend.Advance(Context(m_agent), program, brain.State(), 1.0f, 0) == TickResult::Abandon;
        };

        EXPECT_TRUE(overtaken(0.0));
        EXPECT_FALSE(overtaken(0.2));
    }

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Drawing.

    //! The bug this exists to stop: agents drawing from one shared stream in the same tick come
    //! out in step, which is the identical crowd that drawing at all was meant to break up.
    TEST_F(UtilityRunningFixture, Pick_GivesTwoAgentsDifferentDrawsInTheSameTick)
    {
        DeclareFloat("a", 0.5f);
        DeclareFloat("b", 0.5f);

        AuthoredNode root = Program();
        Text(root, "pick", "weighted");
        Number(root, "top", 2.0);
        AuthoredNode first = Choice("First");
        Considers(first, "a");
        AuthoredNode second = Choice("Second", "shout");
        Considers(second, "b");
        root.m_children.push_back(first);
        root.m_children.push_back(second);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);

        // A slot of its own: agent storage is keyed by index, so reusing one would take this
        // agent's own variables away from it rather than adding a second set.
        const AgentId other(1, 1);
        m_blackboard->CreateAgentBlackboard(other);
        m_blackboard->Set<float>(m_blackboard->FindKey(AZ_NAME_LITERAL("a")), 0.5f, other);
        m_blackboard->Set<float>(m_blackboard->FindKey(AZ_NAME_LITERAL("b")), 0.5f, other);

        Brain mine;
        Brain theirs;
        backend.Attach(Context(m_agent), program, mine.State());
        backend.Attach(Context(other), program, theirs.State());

        AZStd::string ours;
        AZStd::string others;
        int firstPicks = 0;
        for (int i = 0; i < 200; ++i)
        {
            ActionPlan plan;
            const Decision a = backend.Decide(Context(m_agent), program, mine.State(), ActionResult::Success, 0.0f, plan);
            m_store.Release(plan.m_span);
            const auto* mineCursor = reinterpret_cast<const UtilityCursor*>(mine.m_bytes.data());
            ours += static_cast<char>('0' + mineCursor->m_choice);
            firstPicks += mineCursor->m_choice == 0 ? 1 : 0;
            EXPECT_TRUE(a.m_planned);

            ActionPlan theirPlan;
            backend.Decide(Context(other), program, theirs.State(), ActionResult::Success, 0.0f, theirPlan);
            m_store.Release(theirPlan.m_span);
            others += static_cast<char>('0' + reinterpret_cast<const UtilityCursor*>(theirs.m_bytes.data())->m_choice);
        }

        EXPECT_NE(ours, others) << "two agents drew the same sequence, so a crowd would move in step";

        // Both were in the running the whole time, so neither should have taken nearly all of it.
        EXPECT_GT(firstPicks, 40);
        EXPECT_LT(firstPicks, 160);
    }

    //! The same agent, run again, does the same thing: a draw is not a source of drift.
    TEST_F(UtilityRunningFixture, Pick_ReplaysTheSameDrawsForTheSameAgent)
    {
        DeclareFloat("a", 0.5f);
        DeclareFloat("b", 0.5f);

        AuthoredNode root = Program();
        Text(root, "pick", "weighted");
        Number(root, "top", 2.0);
        AuthoredNode first = Choice("First");
        Considers(first, "a");
        AuthoredNode second = Choice("Second", "shout");
        Considers(second, "b");
        root.m_children.push_back(first);
        root.m_children.push_back(second);

        const UtilityProgram program = Built(root);
        UtilityBackend backend(*m_host, *m_blackboard);

        auto sequence = [&]()
        {
            Brain brain;
            backend.Attach(Context(m_agent), program, brain.State());
            AZStd::string drawn;
            for (int i = 0; i < 50; ++i)
            {
                ActionPlan plan;
                backend.Decide(Context(m_agent), program, brain.State(), ActionResult::Success, 0.0f, plan);
                m_store.Release(plan.m_span);
                drawn += static_cast<char>(
                    '0' + reinterpret_cast<const UtilityCursor*>(brain.m_bytes.data())->m_choice);
            }
            return drawn;
        };

        EXPECT_EQ(sequence(), sequence());
    }
} // namespace GOAT
