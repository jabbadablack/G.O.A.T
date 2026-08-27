#include <Backends/Htn/HtnBackend.h>
#include <Backends/Htn/HtnCompiler.h>
#include <Core/Application/AgentRuntime.h>
#include <Core/Scripting/LuaNodeScripting.h>
#include <Backends/Htn/HtnPlanner.h>
#include <Core/Application/BlackboardSystem.h>
#include <Core/Application/NodeTypeRegistry.h>
#include <TestAgentSystem.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    class NamedAction final : public IActionState
    {
    public:
        AZ_RTTI(NamedAction, "{3F81B0C6-27D4-4A19-95E0-6C41A8D3B072}", IActionState);

        explicit NamedAction(const char* name)
            : m_name(name)
        {
        }

        AZ::Name GetName() const override { return m_name; }
        ActionResult Step(const ActionContext&, float) override { return ActionResult::Success; }

        AZ::Name m_name;
    };

    //! Builds domains by hand, so each test states the exact network it means.
    class HtnFixture : public UnitTest::LeakDetectionFixture
    {
    protected:
        void SetUp() override
        {
            UnitTest::LeakDetectionFixture::SetUp();
            AZ::NameDictionary::Create();

            m_blackboard = AZStd::make_unique<BlackboardSystem>();
            m_actions = AZStd::make_unique<ActionStateRegistry>();
            m_actions->Register(AZStd::make_unique<NamedAction>("wait"));
            m_actions->Register(AZStd::make_unique<NamedAction>("shout"));

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

        BlackboardKey DeclareBool(const char* name, bool value)
        {
            const auto declared = m_blackboard->Declare(AZ::Name(name), BlackboardScope::Agent, BlackboardType::Bool);
            EXPECT_TRUE(declared.IsSuccess());
            m_blackboard->Set<bool>(declared.GetValue(), value, m_agent);
            return declared.GetValue();
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

        //! `primitive "name" { <verb>, condition..., effect... }`
        static AuthoredNode Primitive(const char* name, const char* verb)
        {
            AuthoredNode task = Node("primitive");
            Text(task, "name", name);
            task.m_children.push_back(Node(verb));
            return task;
        }

        static AuthoredNode Condition(const char* key, bool expected = true)
        {
            AuthoredNode node = Node("condition");
            Text(node, "key", key);
            Flag(node, "is", expected);
            return node;
        }

        static AuthoredNode Effect(const char* key, bool value = true)
        {
            AuthoredNode node = Node("effect");
            Text(node, "key", key);
            Flag(node, "is", value);
            return node;
        }

        static AuthoredNode Subtask(const char* name)
        {
            AuthoredNode node = Node("subtask");
            Text(node, "task", name);
            return node;
        }

        AZ::Outcome<HtnDomain, AZStd::string> Compile(const AuthoredNode& root) const
        {
            const HtnCompiler compiler(*m_host, *m_blackboard);
            return compiler.Compile(AZ::Name("Test"), root);
        }

        //! Plans a compiled domain and hands back the steps themselves.
        HtnPlanBuffer Steps(const HtnDomain& domain) const
        {
            WorkingState state;
            state.Snapshot(domain, *m_blackboard, m_agent);

            HtnPlanBuffer steps;
            const HtnPlanner planner;
            planner.Plan(domain, domain.m_root, state, steps);
            return steps;
        }

        //! Plans a compiled domain and reports the verbs it produced, in order.
        AZStd::vector<AZ::Name> Run(const HtnDomain& domain) const
        {
            WorkingState state;
            state.Snapshot(domain, *m_blackboard, m_agent);

            HtnPlanBuffer steps;
            const HtnPlanner planner;
            AZStd::vector<AZ::Name> verbs;
            if (!planner.Plan(domain, domain.m_root, state, steps))
            {
                return verbs;
            }

            for (const ActionRequest& step : steps)
            {
                verbs.push_back(m_actions->Find(step.m_action)->GetName());
            }
            return verbs;
        }

        AgentId m_agent;
        AZStd::unique_ptr<BlackboardSystem> m_blackboard;
        AZStd::unique_ptr<ActionStateRegistry> m_actions;
        AZStd::unique_ptr<NodeTypeRegistry> m_nodeTypes;
        AZStd::unique_ptr<TestAgentSystem> m_host;
    };

    TEST_F(HtnFixture, Plan_DecomposesACompoundTaskIntoItsSubtasks)
    {
        DeclareBool("ready", true);

        AuthoredNode root = Node("domain");
        AuthoredNode engage = Node("task");
        Text(engage, "name", "Engage");
        AuthoredNode method = Node("method");
        method.m_children.push_back(Condition("ready"));
        method.m_children.push_back(Subtask("Shout"));
        method.m_children.push_back(Subtask("Rest"));
        engage.m_children.push_back(method);
        root.m_children.push_back(engage);
        root.m_children.push_back(Primitive("Shout", "shout"));
        root.m_children.push_back(Primitive("Rest", "wait"));

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        const AZStd::vector<AZ::Name> verbs = Run(compiled.GetValue());
        ASSERT_EQ(verbs.size(), 2u);
        EXPECT_EQ(verbs[0], AZ::Name("shout"));
        EXPECT_EQ(verbs[1], AZ::Name("wait"));
    }

    //! Methods are tried in the order they were written, so order is priority.
    TEST_F(HtnFixture, Plan_TakesTheFirstMethodWhoseConditionsHold)
    {
        DeclareBool("loud", false);

        AuthoredNode root = Node("domain");
        AuthoredNode engage = Node("task");
        Text(engage, "name", "Engage");

        AuthoredNode first = Node("method");
        first.m_children.push_back(Condition("loud"));
        first.m_children.push_back(Subtask("Shout"));
        engage.m_children.push_back(first);

        AuthoredNode fallback = Node("method");
        fallback.m_children.push_back(Subtask("Rest"));
        engage.m_children.push_back(fallback);

        root.m_children.push_back(engage);
        root.m_children.push_back(Primitive("Shout", "shout"));
        root.m_children.push_back(Primitive("Rest", "wait"));

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        const AZStd::vector<AZ::Name> verbs = Run(compiled.GetValue());
        ASSERT_EQ(verbs.size(), 1u);
        EXPECT_EQ(verbs[0], AZ::Name("wait"));
    }

    //! The property a tree cannot express: a step's effect is visible to a later condition,
    //! so the planner reasons about a world its own plan will have changed.
    TEST_F(HtnFixture, Plan_LetsAnEffectSatisfyALaterCondition)
    {
        DeclareBool("warmed", false);

        AuthoredNode root = Node("domain");
        AuthoredNode engage = Node("task");
        Text(engage, "name", "Engage");
        AuthoredNode method = Node("method");
        method.m_children.push_back(Subtask("WarmUp"));
        method.m_children.push_back(Subtask("Shout"));
        engage.m_children.push_back(method);
        root.m_children.push_back(engage);

        AuthoredNode warm = Primitive("WarmUp", "wait");
        warm.m_children.push_back(Effect("warmed", true));
        root.m_children.push_back(warm);

        // Only runnable once WarmUp has been planned before it.
        AuthoredNode shout = Primitive("Shout", "shout");
        shout.m_children.push_back(Condition("warmed"));
        root.m_children.push_back(shout);

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        const AZStd::vector<AZ::Name> verbs = Run(compiled.GetValue());
        ASSERT_EQ(verbs.size(), 2u);
        EXPECT_EQ(verbs[0], AZ::Name("wait"));
        EXPECT_EQ(verbs[1], AZ::Name("shout"));
    }

    //! A method whose subtask turns out to be unrunnable is taken back, effects and all.
    TEST_F(HtnFixture, Plan_BacktracksToTheNextMethodWhenASubtaskCannotRun)
    {
        DeclareBool("blocked", false);

        AuthoredNode root = Node("domain");
        AuthoredNode engage = Node("task");
        Text(engage, "name", "Engage");

        // Looks fine until its second step is reached, which is what forces the backtrack.
        AuthoredNode first = Node("method");
        first.m_children.push_back(Subtask("WarmUp"));
        first.m_children.push_back(Subtask("Shout"));
        engage.m_children.push_back(first);

        AuthoredNode fallback = Node("method");
        fallback.m_children.push_back(Subtask("Rest"));
        engage.m_children.push_back(fallback);

        root.m_children.push_back(engage);

        AuthoredNode warm = Primitive("WarmUp", "wait");
        warm.m_children.push_back(Effect("blocked", true));
        root.m_children.push_back(warm);

        AuthoredNode shout = Primitive("Shout", "shout");
        shout.m_children.push_back(Condition("blocked", false));
        root.m_children.push_back(shout);
        root.m_children.push_back(Primitive("Rest", "wait"));

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        // The first method's WarmUp step is gone from the plan, not merely followed by a repair.
        const AZStd::vector<AZ::Name> verbs = Run(compiled.GetValue());
        ASSERT_EQ(verbs.size(), 1u);
        EXPECT_EQ(verbs[0], AZ::Name("wait"));
    }

    TEST_F(HtnFixture, Plan_AnswersNothingWhenNoMethodHolds)
    {
        DeclareBool("ready", false);

        AuthoredNode root = Node("domain");
        AuthoredNode engage = Node("task");
        Text(engage, "name", "Engage");
        AuthoredNode method = Node("method");
        method.m_children.push_back(Condition("ready"));
        method.m_children.push_back(Subtask("Shout"));
        engage.m_children.push_back(method);
        root.m_children.push_back(engage);
        root.m_children.push_back(Primitive("Shout", "shout"));

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();
        EXPECT_TRUE(Run(compiled.GetValue()).empty());
    }

    //! Recursion is the point, so the depth cap is what has to stop a runaway one.
    TEST_F(HtnFixture, Plan_RefusesADomainThatRecursesWithoutEnd)
    {
        AuthoredNode root = Node("domain");
        AuthoredNode forever = Node("task");
        Text(forever, "name", "Forever");
        AuthoredNode method = Node("method");
        method.m_children.push_back(Subtask("Forever"));
        forever.m_children.push_back(method);
        root.m_children.push_back(forever);

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        AZ_TEST_START_TRACE_SUPPRESSION;
        EXPECT_TRUE(Run(compiled.GetValue()).empty());
        AZ_TEST_STOP_TRACE_SUPPRESSION_NO_COUNT;
    }

    TEST_F(HtnFixture, Compile_RefusesAnUndeclaredVariable)
    {
        AuthoredNode root = Node("domain");
        AuthoredNode engage = Node("task");
        Text(engage, "name", "Engage");
        AuthoredNode method = Node("method");
        method.m_children.push_back(Condition("never_declared"));
        method.m_children.push_back(Subtask("Shout"));
        engage.m_children.push_back(method);
        root.m_children.push_back(engage);
        root.m_children.push_back(Primitive("Shout", "shout"));

        const auto compiled = Compile(root);
        EXPECT_FALSE(compiled.IsSuccess());
    }

    TEST_F(HtnFixture, Compile_RefusesASubtaskThatIsNotInTheDomain)
    {
        AuthoredNode root = Node("domain");
        AuthoredNode engage = Node("task");
        Text(engage, "name", "Engage");
        AuthoredNode method = Node("method");
        method.m_children.push_back(Subtask("Missing"));
        engage.m_children.push_back(method);
        root.m_children.push_back(engage);

        const auto compiled = Compile(root);
        EXPECT_FALSE(compiled.IsSuccess());
    }

    //! The seam, end to end: an agent whose program is a task network, driven by the same
    //! runtime and the same state machine as a behaviour tree, with no tree anywhere.
    TEST_F(HtnFixture, Agent_RunsWithNoBehaviourTreeAnywhere)
    {
        DeclareBool("ready", true);

        AuthoredNode root = Node("domain");
        AuthoredNode engage = Node("task");
        Text(engage, "name", "Engage");
        AuthoredNode method = Node("method");
        method.m_children.push_back(Condition("ready"));
        method.m_children.push_back(Subtask("Shout"));
        engage.m_children.push_back(method);
        root.m_children.push_back(engage);
        root.m_children.push_back(Primitive("Shout", "shout"));

        HtnBackend backend(*m_host, *m_blackboard);
        auto compiled = backend.Compile(AZ::Name("Soldier"), root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        BackendRegistry backends("backend");
        PlanStore planStore;
        LuaDispatch dispatch;
        AgentScriptContext scriptContext;
        LuaNodeScripting scripting(dispatch, scriptContext);
        AgentRuntime runtime(*m_blackboard, *m_actions, backends, scripting, planStore);

        auto archetype = AZStd::shared_ptr<AgentArchetype>(aznew AgentArchetype());
        archetype->Add(AZ::Name("Soldier"), compiled.GetValue());

        AgentRecord agent;
        agent.m_id = m_agent;
        agent.m_entity = AZ::EntityId(7);
        agent.m_archetype = archetype;
        agent.m_program = archetype->GetProgram(0);

        ASSERT_NE(agent.GetBackend(), nullptr);
        EXPECT_EQ(agent.GetBackend()->GetName(), AZ::Name("htn"));

        runtime.Tick(agent, 0.1f);
        EXPECT_TRUE(agent.m_machine.HasPlan());

        ASSERT_NE(agent.m_machine.GetCurrentAction(), nullptr);
        EXPECT_EQ(m_actions->Find(agent.m_machine.GetCurrentAction()->m_action)->GetName(), AZ::Name("shout"));

        agent.m_machine.ReleasePlan();
        agent.m_archetype.reset();
    }

    //! A primitive runs its verb through that word's own declared properties, so a network can
    //! reach anything a module contributed rather than the handful the compiler knows by name.
    TEST_F(HtnFixture, Compile_CarriesAModuleWordsPropertiesIntoTheRequest)
    {
        NodeTypeDescriptor order;
        order.m_name = AZ_NAME_LITERAL("order_interrupt");
        order.m_kind = NodeKind::Leaf;
        order.m_op = NodeOp::Action;

        NodeParameter tree;
        tree.m_name = AZ_NAME_LITERAL("tree");
        tree.m_type = BlackboardType::Name;
        tree.m_required = true;
        order.m_parameters.push_back(tree);

        NodeParameter limit;
        limit.m_name = AZ_NAME_LITERAL("limit");
        limit.m_type = BlackboardType::Float;
        order.m_parameters.push_back(limit);

        ASSERT_TRUE(m_nodeTypes->Register(AZStd::move(order)));
        const ActionStateId verb = m_actions->Register(AZStd::make_unique<NamedAction>("order_interrupt"));

        AuthoredNode operation = Node("order_interrupt");
        Text(operation, "tree", "CrowdRally");
        AuthoredProperty count;
        count.m_name = "limit";
        count.m_value = 6.0;
        operation.m_properties.push_back(count);

        AuthoredNode root = Node("domain");
        AuthoredNode marshal = Node("primitive");
        Text(marshal, "name", "Order");
        marshal.m_children.push_back(operation);
        root.m_children.push_back(marshal);

        const auto compiled = Compile(root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();

        const HtnPlanBuffer steps = Steps(compiled.GetValue());
        ASSERT_EQ(steps.size(), 1u);
        EXPECT_EQ(steps[0].m_action, verb);
        EXPECT_EQ(steps[0].m_tag, AZ::Name("CrowdRally"));
        EXPECT_FLOAT_EQ(steps[0].m_amount, 6.0f);
    }
} // namespace GOAT
