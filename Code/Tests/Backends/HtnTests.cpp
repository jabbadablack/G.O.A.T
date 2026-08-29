#include <HtnBackend.h>
#include <HtnCompiler.h>
#include <Core/Application/AgentRuntime.h>
#include <Core/Scripting/LuaNodeScripting.h>
#include <HtnPlanner.h>
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

            for (const AZ::u16 task : steps)
            {
                verbs.push_back(m_actions->Find(domain.m_tasks[task].m_action.m_action)->GetName());
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

    //! An agent's plan says what it will do; the choices say why. Without them a log can only
    //! report the steps, which is the one thing a task network does not explain on its own.
    TEST_F(HtnFixture, Plan_ReportsTheMethodItChose)
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
        const HtnDomain& domain = compiled.GetValue();

        WorkingState state;
        state.Snapshot(domain, *m_blackboard, m_agent);

        HtnPlanBuffer steps;
        HtnChoiceTrail choices;
        const HtnPlanner planner;
        ASSERT_TRUE(planner.Plan(domain, domain.m_root, state, steps, &choices));

        // "loud" is false, so the second method is the one that carried Engage.
        ASSERT_EQ(choices.size(), 1u);
        EXPECT_EQ(domain.m_tasks[choices[0].m_task].m_name, AZ::Name("Engage"));
        EXPECT_EQ(choices[0].m_method, 1u);
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
        agent.ResetBrain(*agent.m_program);

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

        const ActionRequest& request = compiled.GetValue().m_tasks[steps[0]].m_action;
        EXPECT_EQ(request.m_action, verb);
        EXPECT_EQ(request.m_tag, AZ::Name("CrowdRally"));
        EXPECT_FLOAT_EQ(request.m_amount, 6.0f);
    }

    //! Everything a running plan is re-checked against.
    class HtnRunningFixture : public HtnFixture
    {
    protected:
        //! Plans a domain and hands back the backend, its state and whether it planned.
        bool Start(HtnBackend& backend, const HtnDomain& domain, BrainState state)
        {
            PlanContext context;
            context.m_agent = m_agent;
            context.m_blackboard = m_blackboard.get();
            context.m_planStore = &m_planStore;

            ActionPlan plan;
            const Decision decision =
                backend.Decide(context, domain, state, ActionResult::Success, 0.0f, plan);
            m_plan = plan;
            return decision.m_planned;
        }

        TickResult Recheck(HtnBackend& backend, const HtnDomain& domain, BrainState state, size_t step)
        {
            PlanContext context;
            context.m_agent = m_agent;
            context.m_blackboard = m_blackboard.get();
            context.m_planStore = &m_planStore;
            return backend.Advance(context, domain, state, 0.0f, step);
        }

        PlanStore m_planStore;
        ActionPlan m_plan;
    };

    //! The point of the whole thing: another backend writes, and this one's plan stops being
    //! worth running without anybody wiring the two together.
    TEST_F(HtnRunningFixture, Advance_DropsThePlanWhenARemainingStepStopsBeingPossible)
    {
        DeclareBool("ready", true);

        AuthoredNode root = Node("domain");
        AuthoredNode go = Node("task");
        Text(go, "name", "Go");
        AuthoredNode method = Node("method");
        method.m_children.push_back(Subtask("First"));
        method.m_children.push_back(Subtask("Second"));
        go.m_children.push_back(method);
        root.m_children.push_back(go);
        root.m_children.push_back(Primitive("First", "wait"));

        AuthoredNode second = Primitive("Second", "shout");
        second.m_children.push_back(Condition("ready"));
        root.m_children.push_back(second);

        HtnBackend backend(*m_host, *m_blackboard);
        auto compiled = backend.Compile(AZ::Name("Go"), root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();
        const auto& domain = static_cast<const HtnDomain&>(*compiled.GetValue());

        AZStd::array<AZ::u8, 128> bytes{};
        const BrainState state(bytes.data(), bytes.size());
        ASSERT_TRUE(Start(backend, domain, state));

        // Still fine while nothing has moved.
        EXPECT_EQ(Recheck(backend, domain, state, 0), TickResult::Continue);

        m_blackboard->Set<bool>(m_blackboard->FindKey(AZ::Name("ready")), false, m_agent);
        EXPECT_EQ(Recheck(backend, domain, state, 0), TickResult::Abandon);

        m_planStore.Release(m_plan.m_span);
    }

    //! And the trap: a plan whose own step writes the variable its method chose on must not
    //! abandon itself. Only the steps left are re-checked, never the method that picked them.
    TEST_F(HtnRunningFixture, Advance_KeepsThePlanWhenOnlyTheMethodsOwnConditionMoved)
    {
        DeclareBool("flag", true);

        AuthoredNode root = Node("domain");
        AuthoredNode marshal = Node("task");
        Text(marshal, "name", "Marshal");

        AuthoredNode acting = Node("method");
        acting.m_children.push_back(Condition("flag"));
        acting.m_children.push_back(Subtask("Act"));
        acting.m_children.push_back(Subtask("Sense"));
        marshal.m_children.push_back(acting);

        AuthoredNode resting = Node("method");
        resting.m_children.push_back(Subtask("Rest"));
        marshal.m_children.push_back(resting);

        root.m_children.push_back(marshal);
        root.m_children.push_back(Primitive("Act", "shout"));
        root.m_children.push_back(Primitive("Sense", "wait"));
        root.m_children.push_back(Primitive("Rest", "wait"));

        HtnBackend backend(*m_host, *m_blackboard);
        auto compiled = backend.Compile(AZ::Name("Marshal"), root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();
        const auto& domain = static_cast<const HtnDomain&>(*compiled.GetValue());

        AZStd::array<AZ::u8, 128> bytes{};
        const BrainState state(bytes.data(), bytes.size());
        ASSERT_TRUE(Start(backend, domain, state));

        // What Sense does. The method chose on this, and the plan must survive it.
        m_blackboard->Set<bool>(m_blackboard->FindKey(AZ::Name("flag")), false, m_agent);
        EXPECT_EQ(Recheck(backend, domain, state, 0), TickResult::Continue);

        m_planStore.Release(m_plan.m_span);
    }

    //! A step that depends on an earlier step's effect is checked against the world that step
    //! will have left, not the one on the blackboard now.
    TEST_F(HtnRunningFixture, Advance_ReplaysWhatTheStepsBeforeItAssumed)
    {
        DeclareBool("warmed", false);

        AuthoredNode root = Node("domain");
        AuthoredNode go = Node("task");
        Text(go, "name", "Go");
        AuthoredNode method = Node("method");
        method.m_children.push_back(Subtask("WarmUp"));
        method.m_children.push_back(Subtask("Shout"));
        go.m_children.push_back(method);
        root.m_children.push_back(go);

        AuthoredNode warm = Primitive("WarmUp", "wait");
        warm.m_children.push_back(Effect("warmed", true));
        root.m_children.push_back(warm);

        AuthoredNode shout = Primitive("Shout", "shout");
        shout.m_children.push_back(Condition("warmed"));
        root.m_children.push_back(shout);

        HtnBackend backend(*m_host, *m_blackboard);
        auto compiled = backend.Compile(AZ::Name("Go"), root);
        ASSERT_TRUE(compiled.IsSuccess()) << compiled.GetError().c_str();
        const auto& domain = static_cast<const HtnDomain&>(*compiled.GetValue());

        AZStd::array<AZ::u8, 128> bytes{};
        const BrainState state(bytes.data(), bytes.size());
        ASSERT_TRUE(Start(backend, domain, state));

        // `warmed` is false on the blackboard and always will be: the effect is an assumption,
        // never a write. Shout must still be judged runnable.
        EXPECT_EQ(Recheck(backend, domain, state, 1), TickResult::Continue);

        m_planStore.Release(m_plan.m_span);
    }
} // namespace GOAT
