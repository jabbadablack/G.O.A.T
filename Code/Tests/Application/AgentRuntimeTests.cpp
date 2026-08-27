#include <Backends/BehaviorTree/BehaviorTreeBackend.h>
#include <Core/Application/ActionStateRegistry.h>
#include <Core/Application/AgentArchetype.h>
#include <Core/Application/AgentRecord.h>
#include <Core/Application/AgentRuntime.h>
#include <Core/Application/BackendRegistry.h>
#include <Core/Application/BlackboardSystem.h>
#include <Core/Application/NodeTypeRegistry.h>
#include <TestAgentSystem.h>
#include <Core/Scripting/AgentScriptContext.h>
#include <Core/Scripting/LuaDispatch.h>
#include <Core/Scripting/LuaNodeScripting.h>

#include <GOAT/Domain/DecisionProgram.h>
#include <GOAT/Domain/PlanStore.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    //! Forwards to a real blackboard while counting the lookups every read goes through.
    //! Wrapping the interface is what makes "how much work did this tick do" measurable
    //! without putting a counter in the shipping code.
    class CountingBlackboard final : public IBlackboardSystem
    {
    public:
        explicit CountingBlackboard(BlackboardSystem& inner)
            : m_inner(inner)
        {
        }

        size_t TakeLookups()
        {
            const size_t count = m_lookups;
            m_lookups = 0;
            return count;
        }

        AZ::Outcome<BlackboardKey, AZStd::string> Declare(
            const AZ::Name& name, BlackboardScope scope, BlackboardType type, AZStd::any defaultValue = {}) override
        {
            return m_inner.Declare(name, scope, type, AZStd::move(defaultValue));
        }

        BlackboardKey FindKey(const AZ::Name& name) const override { return m_inner.FindKey(name); }
        void CreateAgentBlackboard(AgentId agent) override { m_inner.CreateAgentBlackboard(agent); }
        void DestroyAgentBlackboard(AgentId agent) override { m_inner.DestroyAgentBlackboard(agent); }
        void JoinSquad(AgentId agent, const AZ::Name& squad) override { m_inner.JoinSquad(agent, squad); }
        void LeaveSquad(AgentId agent) override { m_inner.LeaveSquad(agent); }
        AZ::Name GetSquad(AgentId agent) const override { return m_inner.GetSquad(agent); }
        BlackboardStorage* FindSquadStorage(const AZ::Name& squad) override { return m_inner.FindSquadStorage(squad); }
        AZStd::vector<AZ::Name> GetSquadNames() const override { return m_inner.GetSquadNames(); }

        BlackboardStorage* FindStorage(BlackboardScope scope, AgentId agent) override
        {
            ++m_lookups;
            return m_inner.FindStorage(scope, agent);
        }

        const BlackboardStorage* FindStorage(BlackboardScope scope, AgentId agent) const override
        {
            ++m_lookups;
            return m_inner.FindStorage(scope, agent);
        }

    private:
        BlackboardSystem& m_inner;
        mutable size_t m_lookups = 0;
    };

    //! A verb that runs forever and counts its calls, so a test can tell a plan that started
    //! from one that merely looked like it did.
    class HoldingAction final : public IActionState
    {
    public:
        AZ_RTTI(HoldingAction, "{2C3A9E14-6B7D-4A21-9F55-1D0E7B4C8A93}", IActionState);

        AZ::Name GetName() const override { return AZ::Name("hold"); }
        void Begin(const ActionContext&) override { ++m_begins; }
        ActionResult Step(const ActionContext&, float) override { return ActionResult::Running; }
        void End(const ActionContext&) override {}

        int m_begins = 0;
    };

    //! Runs whole ticks against a hand built program. Lua is deliberately never connected:
    //! LuaDispatch guards every entry point on a null context, so a tree of conditions and
    //! action leaves runs end to end without an asset, an entity or a script.
    //! Runs for a second, and asks not to be stepped until that second is up.
    class SleepingAction final : public IActionState
    {
    public:
        AZ_RTTI(SleepingAction, "{9C4A1E70-52B8-4D33-A16F-8E0D3B7C4192}", IActionState);

        AZ::Name GetName() const override { return AZ::Name("sleep"); }

        ActionResult Step(const ActionContext& context, float deltaTime) override
        {
            ++m_steps;
            m_slept += deltaTime;
            if (m_slept >= 1.0f)
            {
                return ActionResult::Success;
            }

            context.m_wake->m_when = WakeWhen::AtTime;
            context.m_wake->m_in = 1.0f - m_slept;
            return ActionResult::Running;
        }

        int m_steps = 0;
        float m_slept = 0.0f;
    };

    class AgentRuntimeFixture : public UnitTest::LeakDetectionFixture
    {
    protected:
        void SetUp() override
        {
            UnitTest::LeakDetectionFixture::SetUp();
            AZ::NameDictionary::Create();

            m_inner = AZStd::make_unique<BlackboardSystem>();
            m_blackboard = AZStd::make_unique<CountingBlackboard>(*m_inner);
            m_actions = AZStd::make_unique<ActionStateRegistry>();
            m_backends = AZStd::make_unique<BackendRegistry>("backend");
            m_dispatch = AZStd::make_unique<LuaDispatch>();
            m_scriptContext = AZStd::make_unique<AgentScriptContext>();
            m_scripting = AZStd::make_unique<LuaNodeScripting>(*m_dispatch, *m_scriptContext);
            m_planStore = AZStd::make_unique<PlanStore>();
            m_nodeTypes = AZStd::make_unique<NodeTypeRegistry>();
            m_host = AZStd::make_unique<TestAgentSystem>(*m_nodeTypes, *m_actions, m_backends.get());
            m_treeBackend = AZStd::make_unique<BehaviorTreeBackend>(*m_host, *m_blackboard);

            m_runtime = AZStd::make_unique<AgentRuntime>(
                *m_blackboard, *m_actions, *m_backends, *m_scripting, *m_planStore);

            auto holding = AZStd::make_unique<HoldingAction>();
            m_holding = holding.get();
            m_holdId = m_actions->Register(AZStd::move(holding));

            auto sleeping = AZStd::make_unique<SleepingAction>();
            m_sleeping = sleeping.get();
            m_sleepId = m_actions->Register(AZStd::move(sleeping));

            m_agent.m_id = AgentId(0, 1);
            m_agent.m_entity = AZ::EntityId(1234);
            m_blackboard->CreateAgentBlackboard(m_agent.m_id);
        }

        void TearDown() override
        {
            m_runtime.reset();
            m_treeBackend.reset();
            m_host.reset();
            m_nodeTypes.reset();
            m_planStore.reset();
            m_scripting.reset();
            m_scriptContext.reset();
            m_dispatch.reset();
            m_backends.reset();
            m_actions.reset();
            m_blackboard.reset();
            m_inner.reset();
            m_agent.m_archetype.reset();
            m_agent.m_program = nullptr;
            AZ::NameDictionary::Destroy();
            UnitTest::LeakDetectionFixture::TearDown();
        }

        //! A sequence guarded by one condition, so a closed guard makes the whole tree finish
        //! with no work and every walk of it reads exactly one blackboard slot.
        void BuildGuardedTree(bool open)
        {
            const auto declared =
                m_blackboard->Declare(AZ::Name("gate"), BlackboardScope::Agent, BlackboardType::Bool);
            ASSERT_TRUE(declared.IsSuccess());
            m_gate = declared.GetValue();
            m_blackboard->Set<bool>(m_gate, open, m_agent.m_id);

            // aznew rather than make_shared: AZStd has no make_shared, which is why the gem
            // constructs its programs the same way.
            DecisionProgram* program = aznew DecisionProgram();
            program->m_name = AZ::Name("Guarded");

            DecisionNode root;
            root.m_op = NodeOp::Sequence;
            root.m_parent = InvalidNodeIndex;
            root.m_firstChild = 1;
            root.m_childCount = 2;
            root.m_subtreeEnd = 3;
            program->m_nodes.push_back(root);

            DecisionNode guard;
            guard.m_op = NodeOp::Condition;
            guard.m_parent = 0;
            guard.m_key = m_gate;
            guard.m_subtreeEnd = 2;
            program->m_nodes.push_back(guard);

            DecisionNode leaf;
            leaf.m_op = NodeOp::Action;
            leaf.m_parent = 0;
            leaf.m_subtreeEnd = 3;
            leaf.m_action.m_action = m_holdId;
            program->m_nodes.push_back(leaf);

            program->m_observedKeys.push_back(m_gate);

            Install(program);
        }

        //! Puts one hand built tree on the agent through an archetype, the way the registry
        //! does, so the record under test is shaped exactly like a real one.
        void Install(DecisionProgram* program)
        {
            program->m_backend = m_treeBackend.get();
            for (const BlackboardKey key : program->m_observedKeys)
            {
                program->m_watchedScopes[static_cast<size_t>(key.GetScope())] = true;
            }

            auto archetype = AZStd::shared_ptr<AgentArchetype>(aznew AgentArchetype());
            archetype->Add(program->m_name, AZStd::shared_ptr<const AgentProgram>(program));

            m_agent.m_archetype = archetype;
            m_agent.m_tree = 0;
            m_agent.m_program = archetype->GetProgram(0);
            m_treeBackend->Attach(m_runtime->MakePlanContext(m_agent), *m_agent.m_program, m_agent.GetState());
        }

        //! The cursor the tree backend keeps in the agent's brain state.
        DecisionCursor& Cursor() { return *reinterpret_cast<DecisionCursor*>(m_agent.m_brainState.data()); }

        BlackboardKey m_gate;
        ActionStateId m_holdId = CoreActions::Invalid;
        HoldingAction* m_holding = nullptr;
        ActionStateId m_sleepId = CoreActions::Invalid;
        SleepingAction* m_sleeping = nullptr;
        AgentRecord m_agent;
        AZStd::unique_ptr<BlackboardSystem> m_inner;
        AZStd::unique_ptr<CountingBlackboard> m_blackboard;
        AZStd::unique_ptr<ActionStateRegistry> m_actions;
        AZStd::unique_ptr<BackendRegistry> m_backends;
        AZStd::unique_ptr<LuaDispatch> m_dispatch;
        AZStd::unique_ptr<AgentScriptContext> m_scriptContext;
        AZStd::unique_ptr<LuaNodeScripting> m_scripting;
        AZStd::unique_ptr<PlanStore> m_planStore;
        AZStd::unique_ptr<NodeTypeRegistry> m_nodeTypes;
        AZStd::unique_ptr<TestAgentSystem> m_host;
        AZStd::unique_ptr<BehaviorTreeBackend> m_treeBackend;
        AZStd::unique_ptr<AgentRuntime> m_runtime;
    };

    //! An agent whose tree finds no work must walk it once, not twice. Before the redundant
    //! restart was removed this read the guard two times for one tick, forever.
    TEST_F(AgentRuntimeFixture, Tick_WalksAFinishedTreeOnlyOnce)
    {
        BuildGuardedTree(false);

        m_blackboard->TakeLookups();
        m_runtime->Tick(m_agent, 0.033f);

        EXPECT_EQ(m_blackboard->TakeLookups(), 1u);
    }

    //! Ten idle ticks cost one walk between them, not one each. This is the number the whole
    //! wake condition exists to hold down, so it is stated as a total rather than per tick.
    TEST_F(AgentRuntimeFixture, Tick_CostsOneWalkAcrossManyIdleTicks)
    {
        BuildGuardedTree(false);

        m_blackboard->TakeLookups();
        for (int i = 0; i < 10; ++i)
        {
            m_runtime->Tick(m_agent, 0.033f);
        }

        EXPECT_EQ(m_blackboard->TakeLookups(), 1u);
    }

    //! Once a tree has found no work, walking it again cannot find any until something it
    //! reads changes. So the second and later idle ticks must read nothing at all.
    TEST_F(AgentRuntimeFixture, Tick_ReadsNothingWhileDormant)
    {
        BuildGuardedTree(false);
        m_runtime->Tick(m_agent, 0.033f);

        m_blackboard->TakeLookups();
        for (int i = 0; i < 20; ++i)
        {
            m_runtime->Tick(m_agent, 0.033f);
        }

        EXPECT_EQ(m_blackboard->TakeLookups(), 0u);
    }

    //! Dormancy must not become deafness: a write to an observed slot has to wake the agent.
    TEST_F(AgentRuntimeFixture, Tick_WakesWhenAnObservedSlotChanges)
    {
        BuildGuardedTree(false);
        m_agent.m_observer.Connect(*m_agent.m_program, *m_blackboard, m_agent.m_id);

        m_runtime->Tick(m_agent, 0.033f);
        m_runtime->Tick(m_agent, 0.033f);
        EXPECT_FALSE(m_agent.m_machine.HasPlan());

        m_blackboard->Set<bool>(m_gate, true, m_agent.m_id);
        m_runtime->Tick(m_agent, 0.033f);

        EXPECT_TRUE(m_agent.m_machine.HasPlan());
        m_agent.m_observer.Disconnect();
    }

    //! A handle from ctx:Key has to read back the slot it named. This is the contract the whole
    //! script API now rests on, so it is worth stating on its own.
    TEST_F(AgentRuntimeFixture, Key_ReadsBackTheSlotItNamed)
    {
        const auto declared =
            m_blackboard->Declare(AZ::Name("speed"), BlackboardScope::Agent, BlackboardType::Float);
        ASSERT_TRUE(declared.IsSuccess());

        m_scriptContext->Bind(m_agent.m_id, m_agent.m_entity, m_blackboard.get());

        const double handle = m_scriptContext->Key("speed");
        m_scriptContext->SetNumber(handle, 2.5);

        EXPECT_DOUBLE_EQ(m_scriptContext->GetNumber(handle), 2.5);
        m_scriptContext->Unbind();
    }

    //! A handle Lua never got -- an absent argument arrives as zero -- must not resolve to a
    //! real slot. Zero is a legal packed key, so it has to be kept out of the handle space.
    TEST_F(AgentRuntimeFixture, Key_RefusesAHandleItNeverGaveOut)
    {
        const auto declared =
            m_blackboard->Declare(AZ::Name("flag"), BlackboardScope::Global, BlackboardType::Bool);
        ASSERT_TRUE(declared.IsSuccess());
        ASSERT_EQ(declared.GetValue().GetPacked(), 0u) << "this test only means something if zero is a real key";

        m_scriptContext->Bind(m_agent.m_id, m_agent.m_entity, m_blackboard.get());
        m_scriptContext->SetBool(m_scriptContext->Key("flag"), true);

        // What an absent Lua argument becomes.
        EXPECT_FALSE(m_scriptContext->GetBool(0.0));
        m_scriptContext->Unbind();
    }

    //! A cooldown is the one thing that makes an idle tree runnable again with no blackboard
    //! write at all, so the agent has to come back by itself when it expires. Getting this
    //! wrong is not a slow agent, it is one that never runs again.
    TEST_F(AgentRuntimeFixture, Tick_WakesWhenACooldownExpires)
    {
        DecisionProgram* program = aznew DecisionProgram();
        program->m_name = AZ::Name("Cooling");

        DecisionNode root;
        root.m_op = NodeOp::Cooldown;
        root.m_parent = InvalidNodeIndex;
        root.m_firstChild = 1;
        root.m_childCount = 1;
        root.m_subtreeEnd = 2;
        root.m_amount = 1.0f;
        root.m_cursorSlot = 0;
        program->m_nodes.push_back(root);

        DecisionNode leaf;
        leaf.m_op = NodeOp::Action;
        leaf.m_parent = 0;
        leaf.m_subtreeEnd = 2;
        leaf.m_action.m_action = m_holdId;
        program->m_nodes.push_back(leaf);

        program->m_cursorSlotCount = 1;
        Install(program);

        // Already cooling, with half a second left to run. Slot zero is the cooldown's, since
        // it is the only node in this tree that keeps anything between ticks.
        Cursor().Slot(0) = 0.5f;

        m_runtime->Tick(m_agent, 0.1f);
        EXPECT_FALSE(m_agent.m_machine.HasPlan());

        // Still cooling: the agent stays asleep rather than re-walking a tree that cannot move.
        m_runtime->Tick(m_agent, 0.1f);
        EXPECT_FALSE(m_agent.m_machine.HasPlan());

        // Past the deadline, and nothing wrote to the blackboard to say so.
        m_runtime->Tick(m_agent, 0.5f);
        EXPECT_TRUE(m_agent.m_machine.HasPlan());
    }

    //! An open guard still has to produce work, so the saving above is not simply the agent
    //! having stopped running.
    TEST_F(AgentRuntimeFixture, Tick_StartsAPlanWhenTheGuardIsOpen)
    {
        BuildGuardedTree(true);

        m_runtime->Tick(m_agent, 0.033f);
        EXPECT_TRUE(m_agent.m_machine.HasPlan());

        // The verb begins on the tick after the plan starts: starting one ends that tick, and
        // the state machine steps the action at the top of the next.
        EXPECT_EQ(m_holding->m_begins, 0);
        m_runtime->Tick(m_agent, 0.033f);
        EXPECT_EQ(m_holding->m_begins, 1);
    }

    //! An action that says when it next has something to do is left alone until then, rather
    //! than being stepped every band tick to be told nothing changed.
    TEST_F(AgentRuntimeFixture, Tick_LeavesASleepingActionAlone)
    {
        BuildGuardedTree(true);
        const_cast<DecisionProgram*>(static_cast<const DecisionProgram*>(m_agent.m_program))
            ->m_nodes[2].m_action.m_action = m_sleepId;

        for (int i = 0; i < 12; ++i)
        {
            m_runtime->Tick(m_agent, 0.1f);
        }

        // One step to arm the sleep and one when the second is up, rather than one per tick.
        EXPECT_LE(m_sleeping->m_steps, 3);
        EXPECT_GT(m_sleeping->m_steps, 0);
    }

    //! The sleep is a bound, not a promise: something that knows better can end it early.
    TEST_F(AgentRuntimeFixture, Wake_EndsASleepEarly)
    {
        BuildGuardedTree(true);
        const_cast<DecisionProgram*>(static_cast<const DecisionProgram*>(m_agent.m_program))
            ->m_nodes[2].m_action.m_action = m_sleepId;

        m_runtime->Tick(m_agent, 0.1f);
        m_runtime->Tick(m_agent, 0.1f);
        const int armed = m_sleeping->m_steps;

        m_agent.m_wakeIn = 0.0f;
        m_runtime->Tick(m_agent, 0.1f);

        EXPECT_GT(m_sleeping->m_steps, armed);
    }
} // namespace GOAT
