#include <AzCore/Console/LoggerSystemComponent.h>
#include <AzCore/EBus/EventSchedulerSystemComponent.h>
#include <AzCore/Time/TimeSystem.h>

#include <Core/Application/AgentArchetype.h>
#include <Core/Application/AgentRegistry.h>
#include <Core/Application/AgentRuntime.h>
#include <Core/Application/BackendRegistry.h>
#include <Core/Application/BlackboardSystem.h>
#include <Core/Application/NodeTypeRegistry.h>
#include <TestAgentSystem.h>
#include <Core/Scripting/LuaNodeScripting.h>
#include <Core/Actions/EmbedAction.h>
#include <Core/Application/AgentRecord.h>
#include <Core/Application/GuardWatch.h>
#include <BehaviorTreeBackend.h>
#include <UtilityBackend.h>
#include <DecisionCursor.h>
#include <TreeWalker.h>

#include <GOAT_BehaviorTree/DecisionProgram.h>
#include <GOAT/Domain/PlanStore.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

#if defined(HAVE_BENCHMARK)

namespace GOAT::Benchmark
{
    //! Measures the decision path with no engine around it: no entities, no components, no
    //! assets and no Lua. DecisionProgram is a plain struct, so a benchmark can build the tree
    //! it means directly, which keeps these numbers about the gem rather than about O3DE.
    class AgentBenchmarkFixture : public UnitTest::AllocatorsBenchmarkFixture
    {
    public:
        // Both signatures, because the base overrides both and Google Benchmark calls the non
        // const one. Overriding only the const form leaves it hidden and never run, which shows
        // up as a crash in the first AZ::Name the benchmark builds.
        void SetUp(const ::benchmark::State& state) override
        {
            UnitTest::AllocatorsBenchmarkFixture::SetUp(state);
            Init();
        }

        void SetUp(::benchmark::State& state) override
        {
            UnitTest::AllocatorsBenchmarkFixture::SetUp(state);
            Init();
        }

        void TearDown(const ::benchmark::State& state) override
        {
            Shutdown();
            UnitTest::AllocatorsBenchmarkFixture::TearDown(state);
        }

        void TearDown(::benchmark::State& state) override
        {
            Shutdown();
            UnitTest::AllocatorsBenchmarkFixture::TearDown(state);
        }

    protected:
        void Init()
        {
            AZ::NameDictionary::Create();
            m_blackboard = AZStd::make_unique<BlackboardSystem>();
        }

        void Shutdown()
        {
            // Capacity released, not just cleared: the base fixture checks that a benchmark
            // leaves no allocation behind, and clear() on an AZStd::vector keeps its buffer.
            m_agents.set_capacity(0);
            m_cursors.set_capacity(0);
            m_program.reset();
            m_blackboard.reset();
            AZ::NameDictionary::Destroy();
        }

        //! A tree shaped like an authored one: a selector over `branches` guarded sequences,
        //! the last of which is open, so a walk descends past the closed guards and produces
        //! work. That is the fully active workload rather than an idle one.
        void BuildProgram(int branches)
        {
            m_gate = m_blackboard->Declare(AZ::Name("gate"), BlackboardScope::Agent, BlackboardType::Bool)
                         .GetValue();

            // Every branch but the last gets a gate of its own, left closed, so a walk descends
            // through all of them before finding work. Sharing one key would let the first branch
            // always win and the tree's width would never be walked at all.
            m_closed = m_blackboard->Declare(AZ::Name("closed"), BlackboardScope::Agent, BlackboardType::Bool)
                           .GetValue();

            m_program = AZStd::make_unique<DecisionProgram>();
            m_program->m_name = AZ::Name("Bench");

            DecisionNode root;
            root.m_op = NodeOp::Selector;
            root.m_parent = InvalidNodeIndex;
            m_program->m_nodes.push_back(root);

            for (int i = 0; i < branches; ++i)
            {
                const NodeIndex sequence = static_cast<NodeIndex>(m_program->m_nodes.size());
                DecisionNode branch;
                branch.m_op = NodeOp::Sequence;
                branch.m_parent = 0;
                m_program->m_nodes.push_back(branch);

                DecisionNode guard;
                guard.m_op = NodeOp::Condition;
                guard.m_parent = sequence;
                guard.m_key = (i == branches - 1) ? m_gate : m_closed;
                m_program->m_nodes.push_back(guard);

                DecisionNode leaf;
                leaf.m_op = NodeOp::Action;
                leaf.m_parent = sequence;
                leaf.m_action.m_amount = 1.0f;
                m_program->m_nodes.push_back(leaf);
            }

            CloseSubtrees();
            m_program->m_observedKeys.push_back(m_gate);
        }

        //! Fills in the pre-order extents the walker steps between siblings with.
        void CloseSubtrees()
        {
            const NodeIndex count = static_cast<NodeIndex>(m_program->m_nodes.size());
            for (NodeIndex i = 0; i < count; ++i)
            {
                m_program->m_nodes[i].m_subtreeEnd = i + 1;
                m_program->m_nodes[i].m_firstChild = InvalidNodeIndex;
                m_program->m_nodes[i].m_childCount = 0;
            }

            for (NodeIndex i = count; i > 0; --i)
            {
                DecisionNode& node = m_program->m_nodes[i - 1];
                if (node.m_parent == InvalidNodeIndex)
                {
                    continue;
                }

                DecisionNode& parent = m_program->m_nodes[node.m_parent];
                parent.m_subtreeEnd = AZStd::max(parent.m_subtreeEnd, node.m_subtreeEnd);
                parent.m_childCount++;
                if (parent.m_firstChild == InvalidNodeIndex || (i - 1) < parent.m_firstChild)
                {
                    parent.m_firstChild = i - 1;
                }
            }
        }

        //! Registers `count` agents, all running the one program, all with the gate open so
        //! every one of them is doing work on every tick.
        void SpawnAgents(int count)
        {
            m_agents.reserve(static_cast<size_t>(count));
            m_cursors.resize(static_cast<size_t>(count));

            for (int i = 0; i < count; ++i)
            {
                const AgentId agent(static_cast<AZ::u32>(i), 1);
                m_blackboard->CreateAgentBlackboard(agent);
                m_blackboard->Set<bool>(m_gate, true, agent);
                m_agents.push_back(agent);
                m_cursors[static_cast<size_t>(i)].Reset(*m_program);
            }
        }

        PlanContext ContextFor(AgentId agent)
        {
            PlanContext context;
            context.m_agent = agent;
            context.m_blackboard = m_blackboard.get();
            context.m_planStore = m_planStore.get();
            return context;
        }

        //! Reports what one agent costs in bytes. Only what this fixture allocates is counted --
        //! its blackboard storage and its cursor -- because the benchmark drives the decision
        //! path directly rather than through AgentRegistry. An allocation count belongs here too
        //! and is deliberately absent until it can be taken from the allocator rather than
        //! guessed, because a fabricated zero is worse than no number at all.
        static void ReportPerAgent(::benchmark::State& state, size_t bytes, int agents)
        {
            state.counters["bytes/agent"] =
                ::benchmark::Counter(static_cast<double>(bytes) / static_cast<double>(agents));
        }

        BlackboardKey m_gate;
        BlackboardKey m_closed;
        AZStd::unique_ptr<DecisionProgram> m_program;
        AZStd::unique_ptr<BlackboardSystem> m_blackboard;
        AZStd::vector<AgentId> m_agents;
        AZStd::vector<DecisionCursor> m_cursors;
        AZStd::unique_ptr<PlanStore> m_planStore = AZStd::make_unique<PlanStore>();
        TreeWalker m_walker;
    };

    //! Every agent walks its tree and produces work. This is the headline number.
    BENCHMARK_DEFINE_F(AgentBenchmarkFixture, BM_TickRunning)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        BuildProgram(8);
        SpawnAgents(agents);

        for ([[maybe_unused]] auto _ : state)
        {
            for (int i = 0; i < agents; ++i)
            {
                DecisionCursor& cursor = m_cursors[static_cast<size_t>(i)];
                cursor.AdvanceClock(0.033f);
                ::benchmark::DoNotOptimize(m_walker.Begin(*m_program, cursor, ContextFor(m_agents[static_cast<size_t>(i)])));
            }
        }

        state.SetItemsProcessed(state.iterations() * agents);
    }

    BENCHMARK_REGISTER_F(AgentBenchmarkFixture, BM_TickRunning)->Arg(100)->Arg(1000)->Arg(10000);

    //! One walk over a wide tree, with no registry and no agents around it, so the cursor's
    //! own cost is what is being measured.
    BENCHMARK_DEFINE_F(AgentBenchmarkFixture, BM_TreeWalk)(::benchmark::State& state)
    {
        BuildProgram(static_cast<int>(state.range(0)));
        SpawnAgents(1);

        DecisionCursor& cursor = m_cursors[0];
        for ([[maybe_unused]] auto _ : state)
        {
            cursor.AdvanceClock(0.033f);
            ::benchmark::DoNotOptimize(m_walker.Begin(*m_program, cursor, ContextFor(m_agents[0])));
        }
    }

    BENCHMARK_REGISTER_F(AgentBenchmarkFixture, BM_TreeWalk)->Arg(4)->Arg(30)->Arg(100);

    //! Registration cost, and the per agent memory it commits.
    BENCHMARK_DEFINE_F(AgentBenchmarkFixture, BM_SpawnAgents)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        BuildProgram(8);

        size_t bytes = 0;

        for ([[maybe_unused]] auto _ : state)
        {
            state.PauseTiming();
            // Destroyed before the replacement is built: only one blackboard system may exist,
            // and assigning over it would construct the new one while the old still holds the slot.
            m_blackboard.reset();
            m_blackboard = AZStd::make_unique<BlackboardSystem>();
            m_gate = m_blackboard->Declare(AZ::Name("gate"), BlackboardScope::Agent, BlackboardType::Bool)
                         .GetValue();
            m_agents.clear();
            m_cursors.clear();
            const auto& allocator = AZ::AllocatorInstance<AZ::SystemAllocator>::Get();
            const size_t bytesBefore = allocator.NumAllocatedBytes();
            state.ResumeTiming();

            SpawnAgents(agents);

            state.PauseTiming();
            bytes = allocator.NumAllocatedBytes() - bytesBefore;
            state.ResumeTiming();
        }

        ReportPerAgent(state, bytes, agents);
        state.SetItemsProcessed(state.iterations() * agents);
    }

    BENCHMARK_REGISTER_F(AgentBenchmarkFixture, BM_SpawnAgents)->Arg(100)->Arg(1000)->Arg(10000);

    //! One write to a Global slot that every agent's tree guards on. The number that matters is
    //! not the absolute time but whether it grows with the agent count: a write that has to tell
    //! each watcher is O(agents), and a write that bumps a counter they read for themselves is
    //! O(1). Flatness across the three sizes is the whole claim.
    BENCHMARK_DEFINE_F(AgentBenchmarkFixture, BM_GlobalWrite)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        BuildProgram(8);

        const BlackboardKey alarm =
            m_blackboard->Declare(AZ::Name("alarm"), BlackboardScope::Global, BlackboardType::Bool).GetValue();

        auto watched = AZStd::make_unique<DecisionProgram>();
        watched->m_name = AZ::Name("Watching");
        watched->m_nodes = m_program->m_nodes;
        watched->m_observedKeys.push_back(alarm);

        SpawnAgents(agents);

        AZStd::vector<GuardWatch> watches(static_cast<size_t>(agents));
        for (int i = 0; i < agents; ++i)
        {
            watches[static_cast<size_t>(i)].Connect(*watched, *m_blackboard, m_agents[static_cast<size_t>(i)]);
            watches[static_cast<size_t>(i)].Clear();
        }

        bool value = false;
        for ([[maybe_unused]] auto _ : state)
        {
            value = !value;
            m_blackboard->Set<bool>(alarm, value, AgentId{});
        }

        state.counters["bytes/AgentRecord"] = ::benchmark::Counter(static_cast<double>(sizeof(AgentRecord)));
        watches.set_capacity(0);
    }

    BENCHMARK_REGISTER_F(AgentBenchmarkFixture, BM_GlobalWrite)->Arg(100)->Arg(1000)->Arg(10000);

    //! One write to each agent's own slot.
    BENCHMARK_DEFINE_F(AgentBenchmarkFixture, BM_AgentWrite)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        BuildProgram(8);
        SpawnAgents(agents);

        bool value = false;
        for ([[maybe_unused]] auto _ : state)
        {
            value = !value;
            for (int i = 0; i < agents; ++i)
            {
                m_blackboard->Set<bool>(m_gate, value, m_agents[static_cast<size_t>(i)]);
            }
        }

        state.SetItemsProcessed(state.iterations() * agents);
    }

    BENCHMARK_REGISTER_F(AgentBenchmarkFixture, BM_AgentWrite)->Arg(100)->Arg(1000)->Arg(10000);
    //! Registers agents through the real registry, which is where the per agent cost of joining
    //! a level actually lives: a slot, a blackboard, a squad, a guard watch, a band and an
    //! entity index entry. The fixture above measures only the blackboard.
    class AgentRegistryBenchmarkFixture : public AgentBenchmarkFixture
    {
    public:
        void SetUp(const ::benchmark::State& state) override
        {
            AgentBenchmarkFixture::SetUp(state);
            Build();
        }

        void SetUp(::benchmark::State& state) override
        {
            AgentBenchmarkFixture::SetUp(state);
            Build();
        }

        void TearDown(const ::benchmark::State& state) override
        {
            Teardown();
            AgentBenchmarkFixture::TearDown(state);
        }

        void TearDown(::benchmark::State& state) override
        {
            Teardown();
            AgentBenchmarkFixture::TearDown(state);
        }

    protected:
        void Build()
        {
            // AgentRegistry enqueues a scheduled event per band the moment it is built, so a
            // benchmark of it needs the clock and the scheduler those events live on.
            m_logger = AZStd::make_unique<AZ::LoggerSystemComponent>();
            m_time = AZStd::make_unique<AZ::TimeSystem>();
            m_scheduler = AZStd::make_unique<AZ::EventSchedulerSystemComponent>();

            m_blackboard.reset();
            m_blackboard = AZStd::make_unique<BlackboardSystem>();
            m_actions = AZStd::make_unique<ActionStateRegistry>();
            m_backends = AZStd::make_unique<BackendRegistry>("backend");
            m_dispatch = AZStd::make_unique<LuaDispatch>();
            m_luaContext = AZStd::make_unique<AgentScriptContext>();
            m_scripting = AZStd::make_unique<LuaNodeScripting>(*m_dispatch, *m_luaContext);
            m_nodeTypes = AZStd::make_unique<NodeTypeRegistry>();
            m_host = AZStd::make_unique<TestAgentSystem>(*m_nodeTypes, *m_actions, m_backends.get());
            m_treeBackend = AZStd::make_unique<BehaviorTreeBackend>(*m_host, *m_blackboard);
            m_utilityBackend = AZStd::make_unique<UtilityBackend>(*m_host, *m_blackboard);
            m_runtime = AZStd::make_unique<AgentRuntime>(
                *m_blackboard, *m_actions, *m_backends, *m_scripting, *m_planStore);
            m_registry = AZStd::make_unique<AgentRegistry>(*m_runtime, *m_blackboard, *m_dispatch);
        }

        void Teardown()
        {
            // Unregistered rather than dropped, so every plan goes back to the store before the
            // name dictionary is torn down. A released plan is what gives up the names its
            // steps hold, and the store outlives this function.
            if (m_registry != nullptr)
            {
                for (const AgentId agent : m_registry->GetAgents())
                {
                    m_registry->Unregister(agent);
                }
            }

            m_library.clear();
            m_registry.reset();
            m_runtime.reset();
            m_scored.clear();
            m_scored.set_capacity(0);
            m_utilityBackend.reset();
            m_treeBackend.reset();
            m_host.reset();
            m_nodeTypes.reset();
            m_scripting.reset();
            m_luaContext.reset();
            m_dispatch.reset();
            m_backends.reset();
            m_actions.reset();
            m_archetype.reset();
            m_blackboard.reset();
            m_scheduler.reset();
            m_time.reset();
            m_logger.reset();

            // Last, once nothing holds a reference to it. A released block is recycled rather
            // than freed, so the steps in it keep the names they carry until the store itself
            // goes, and a store left to be a member would outlive the name dictionary.
            m_planStore = AZStd::make_unique<PlanStore>();
        }

        //! A verb that never finishes, so an agent that starts one stays busy. Registering it
        //! is what makes the benchmark measure working agents rather than dormant ones.
        class BusyAction final : public IActionState
        {
        public:
            AZ_RTTI(BusyAction, "{6D1F5B2E-4A77-4C09-9E33-70B5A5C1D482}", IActionState);
            AZ::Name GetName() const override { return AZ::Name("busy"); }
            ActionResult Step(const ActionContext&, float) override { return ActionResult::Running; }
        };

        //! A verb that always has a second to wait, so every agent is asleep between services.
        class WaitingAction final : public IActionState
        {
        public:
            AZ_RTTI(WaitingAction, "{1E9D4C05-73A6-42BB-8F41-0D6C2E5A9B38}", IActionState);
            AZ::Name GetName() const override { return AZ::Name("waiting"); }

            ActionResult Step(const ActionContext& context, float) override
            {
                context.m_wake->m_when = WakeWhen::AtTime;
                context.m_wake->m_in = 1.0f;
                return ActionResult::Running;
            }
        };

        //! The same population, but every condition is watched and one of them reads a global
        //! variable. This is what a tree costs now that writing a condition means observing it.
        void MakeReactiveArchetype()
        {
            m_alarm = m_blackboard->Declare(AZ::Name("alarm"), BlackboardScope::Global, BlackboardType::Bool)
                          .GetValue();
            BuildProgram(8);

            const ActionStateId busy = m_actions->Register(AZStd::make_unique<BusyAction>());
            for (DecisionNode& node : m_program->m_nodes)
            {
                if (node.m_op == NodeOp::Action)
                {
                    node.m_action.m_action = busy;
                }
                if (node.m_op == NodeOp::Condition)
                {
                    node.m_abort = AbortMode::Self;
                    m_program->m_guardNodes.push_back(
                        static_cast<NodeIndex>(&node - m_program->m_nodes.data()));
                }
            }

            // The first branch reads the global one, so a write to it is what wakes everybody.
            m_program->m_nodes[2].m_key = m_alarm;
            m_program->m_observedKeys.push_back(m_alarm);

            auto shared = AZStd::shared_ptr<DecisionProgram>(aznew DecisionProgram(*m_program));
            shared->m_backend = m_treeBackend.get();
            shared->m_watchedScopes[static_cast<size_t>(BlackboardScope::Agent)] = true;
            shared->m_watchedScopes[static_cast<size_t>(BlackboardScope::Global)] = true;
            auto archetype = AZStd::shared_ptr<AgentArchetype>(aznew AgentArchetype());
            archetype->Add(AZ::Name("Bench"), AZStd::move(shared));
            m_archetype = archetype;
        }

        //! The same population, but every agent's running step is a one node program of another
        //! kind rather than a verb. Against BM_TickBand this is what one nesting level costs.
        void MakeNestedArchetype()
        {
            const ActionStateId busy = m_actions->Register(AZStd::make_unique<BusyAction>());

            DecisionProgram* inner = aznew DecisionProgram();
            inner->m_name = AZ::Name("Inner");
            inner->m_backend = m_treeBackend.get();
            DecisionNode leaf;
            leaf.m_op = NodeOp::Action;
            leaf.m_parent = InvalidNodeIndex;
            leaf.m_subtreeEnd = 1;
            leaf.m_action.m_action = busy;
            inner->m_nodes.push_back(leaf);
            inner->m_stateBytes = AlignState(m_treeBackend->GetStateSize());
            m_library[inner->m_name] = AZStd::shared_ptr<const AgentProgram>(inner);

            m_actions->RegisterAt(CoreActions::Embed,
                AZStd::make_unique<EmbedAction>(
                    [this](AgentId agent) { return m_registry->Find(agent); }, *m_runtime, *m_actions, m_library));

            BuildProgram(8);
            for (DecisionNode& node : m_program->m_nodes)
            {
                if (node.m_op == NodeOp::Action)
                {
                    node.m_action.m_action = CoreActions::Embed;
                    node.m_action.m_tag = AZ::Name("Inner");
                }
            }

            auto shared = AZStd::shared_ptr<DecisionProgram>(aznew DecisionProgram(*m_program));
            shared->m_backend = m_treeBackend.get();
            shared->m_watchedScopes[static_cast<size_t>(BlackboardScope::Agent)] = true;
            shared->m_stateBytes =
                AlignState(m_treeBackend->GetStateSize()) + NestedFrameBytes() + inner->m_stateBytes;

            auto archetype = AZStd::shared_ptr<AgentArchetype>(aznew AgentArchetype());
            archetype->Add(AZ::Name("Bench"), AZStd::move(shared));
            m_archetype = archetype;
        }

        void MakeArchetype(bool waiting = false)
        {
            BuildProgram(8);
            // Every leaf runs the busy verb, so a walk that reaches one produces a plan the
            // agent keeps running rather than a refusal.
            const ActionStateId busy = waiting
                ? m_actions->Register(AZStd::make_unique<WaitingAction>())
                : m_actions->Register(AZStd::make_unique<BusyAction>());
            for (DecisionNode& node : m_program->m_nodes)
            {
                if (node.m_op == NodeOp::Action)
                {
                    node.m_action.m_action = busy;
                }
            }

            auto shared = AZStd::shared_ptr<DecisionProgram>(aznew DecisionProgram(*m_program));
            shared->m_backend = m_treeBackend.get();
            shared->m_watchedScopes[static_cast<size_t>(BlackboardScope::Agent)] = true;
            auto archetype = AZStd::shared_ptr<AgentArchetype>(aznew AgentArchetype());
            archetype->Add(AZ::Name("Bench"), AZStd::move(shared));
            m_archetype = archetype;
        }

        //! A population deciding by scoring rather than by walking. Every choice argues from a
        //! few numbers, overlapping so that one of them moving changes what wins; that is what
        //! lets the benchmark below prove it is really scoring rather than idling.
        void MakeScoredArchetype(int choices = 8, int considerations = 4)
        {
            const ActionStateId busy = m_actions->Register(AZStd::make_unique<BusyAction>());

            auto program = AZStd::shared_ptr<UtilityProgram>(aznew UtilityProgram());
            program->m_name = AZ::Name("Bench");
            program->m_backend = m_utilityBackend.get();
            program->m_watchedScopes[static_cast<size_t>(BlackboardScope::Agent)] = true;

            // The worst case on purpose: every agent scores every tick. What keeps a real
            // population away from this is dormancy and the recheck floor, and measuring those
            // would be measuring how often it is asked rather than what asking costs.
            program->m_wantsTick = true;
            program->m_recheck = 0.0f;

            for (int c = 0; c < choices + considerations; ++c)
            {
                const auto declared = m_blackboard->Declare(
                    AZ::Name(AZStd::string::format("weight_%d", c)), BlackboardScope::Agent, BlackboardType::Float);
                m_scored.push_back(declared.GetValue());
            }

            for (int i = 0; i < choices; ++i)
            {
                UtilityChoice choice;
                choice.m_name = AZ::Name(AZStd::string::format("Choice_%d", i));
                choice.m_firstConsideration = static_cast<AZ::u16>(program->m_considerations.size());
                choice.m_considerationCount = static_cast<AZ::u16>(considerations);
                for (int c = 0; c < considerations; ++c)
                {
                    program->m_considerations.push_back({ m_scored[static_cast<size_t>(i + c)], false });
                }

                choice.m_firstStep = static_cast<AZ::u16>(program->m_steps.size());
                choice.m_stepCount = 1;
                ActionRequest request;
                request.m_action = busy;
                program->m_steps.push_back(request);

                program->m_choices.push_back(AZStd::move(choice));
            }

            auto archetype = AZStd::shared_ptr<AgentArchetype>(aznew AgentArchetype());
            archetype->Add(AZ::Name("Bench"), AZStd::move(program));
            m_archetype = archetype;
        }

        //! Which choice an agent settled on, read out of its own brain block.
        AZ::u16 ChoiceOf(AgentId agent)
        {
            AgentRecord* record = m_registry->Find(agent);
            return record != nullptr ? reinterpret_cast<const UtilityCursor*>(record->GetState().data())->m_choice
                                     : InvalidChoice;
        }

        AZStd::unique_ptr<AZ::LoggerSystemComponent> m_logger;
        AZStd::unique_ptr<AZ::TimeSystem> m_time;
        AZStd::unique_ptr<AZ::EventSchedulerSystemComponent> m_scheduler;
        AZStd::shared_ptr<const AgentArchetype> m_archetype;
        AZStd::unique_ptr<ActionStateRegistry> m_actions;
        AZStd::unique_ptr<BackendRegistry> m_backends;
        AZStd::unique_ptr<LuaDispatch> m_dispatch;
        AZStd::unique_ptr<AgentScriptContext> m_luaContext;
        AZStd::unique_ptr<LuaNodeScripting> m_scripting;
        AZStd::unique_ptr<NodeTypeRegistry> m_nodeTypes;
        AZStd::unique_ptr<TestAgentSystem> m_host;
        AZStd::unique_ptr<BehaviorTreeBackend> m_treeBackend;
        AZStd::unique_ptr<UtilityBackend> m_utilityBackend;
        AZStd::vector<BlackboardKey> m_scored;
        AZStd::unique_ptr<AgentRuntime> m_runtime;
        BlackboardKey m_alarm;
        AZStd::unique_ptr<AgentRegistry> m_registry;
        EmbedAction::ProgramTable m_library;
    };

    //! Every table grows on its own as agents arrive.
    BENCHMARK_DEFINE_F(AgentRegistryBenchmarkFixture, BM_RegisterAgents)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        for ([[maybe_unused]] auto _ : state)
        {
            state.PauseTiming();
            Teardown();
            Build();
            MakeArchetype();
            state.ResumeTiming();

            for (int i = 0; i < agents; ++i)
            {
                m_registry->Register(AZ::EntityId(static_cast<AZ::u64>(i) + 1), m_archetype, 0, AZ::Name{});
            }
        }

        state.SetItemsProcessed(state.iterations() * agents);
    }

    BENCHMARK_REGISTER_F(AgentRegistryBenchmarkFixture, BM_RegisterAgents)->Arg(100)->Arg(1000)->Arg(10000);

    //! A whole band tick through the registry: the loop, the lookups, and each agent's tick.
    //! This is the number a parallel tick has to move, and the only one that measures the path
    //! the engine actually drives.
    BENCHMARK_DEFINE_F(AgentRegistryBenchmarkFixture, BM_TickBand)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        MakeArchetype();
        m_registry->Reserve(static_cast<size_t>(agents), 0);

        AZStd::vector<AgentId> registered;
        registered.reserve(static_cast<size_t>(agents));
        for (int i = 0; i < agents; ++i)
        {
            registered.push_back(
                m_registry->Register(AZ::EntityId(static_cast<AZ::u64>(i) + 1), m_archetype, 0, AZ::Name{}));
        }

        // Opened so the last branch of every agent's tree produces work: this measures a
        // population that is running something, not one that has settled.
        for (const AgentId agent : registered)
        {
            m_blackboard->Set<bool>(m_gate, true, agent);
        }

        // Two ticks to get everybody past starting a plan and into running one.
        m_registry->TickBand(0);
        m_registry->TickBand(0);

        // A tick that decides nothing is nearly free, so measuring one would report a speed
        // that means nothing. Fail loudly instead.
        size_t running = 0;
        for (const AgentId agent : registered)
        {
            const AgentRecord* record = m_registry->Find(agent);
            running += record != nullptr && record->m_machine.HasPlan() ? 1 : 0;
        }
        if (running != static_cast<size_t>(agents))
        {
            state.SkipWithError("agents are not running anything, so this measures an idle tick");
            return;
        }

        for ([[maybe_unused]] auto _ : state)
        {
            m_registry->TickBand(0);
        }

        state.SetItemsProcessed(state.iterations() * agents);
    }

    //! The same band tick, but every agent's running step is another program rather than a
    //! verb. Its ratio to BM_TickBand is what one level of handing work on actually costs.
    BENCHMARK_DEFINE_F(AgentRegistryBenchmarkFixture, BM_TickNested)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        MakeNestedArchetype();

        AZStd::vector<AgentId> registered;
        registered.reserve(static_cast<size_t>(agents));
        for (int i = 0; i < agents; ++i)
        {
            registered.push_back(
                m_registry->Register(AZ::EntityId(static_cast<AZ::u64>(i) + 1), m_archetype, 0, AZ::Name{}));
        }

        for (const AgentId agent : registered)
        {
            m_blackboard->Set<bool>(m_gate, true, agent);
        }

        // Three ticks: one to plan the host's step, one to enter the nested program and plan
        // its first, and one to get that running.
        m_registry->TickBand(0);
        m_registry->TickBand(0);
        m_registry->TickBand(0);

        size_t nested = 0;
        for (const AgentId agent : registered)
        {
            const AgentRecord* record = m_registry->Find(agent);
            nested += record != nullptr && record->m_brainUsed > AlignState(m_treeBackend->GetStateSize()) ? 1 : 0;
        }
        if (nested != static_cast<size_t>(agents))
        {
            state.SkipWithError("agents are not inside a nested program, so this measures nothing");
            return;
        }

        for ([[maybe_unused]] auto _ : state)
        {
            m_registry->TickBand(0);
        }

        state.SetItemsProcessed(state.iterations() * agents);
    }

    BENCHMARK_REGISTER_F(AgentRegistryBenchmarkFixture, BM_TickNested)->Arg(100)->Arg(1000)->Arg(10000);

    //! The same band tick, but every agent decides by scoring what it could do rather than by
    //! walking a tree to the first thing that will run. Its ratio to BM_TickBand is what a
    //! paradigm that weighs all of its options costs against one that takes the first that fits.
    BENCHMARK_DEFINE_F(AgentRegistryBenchmarkFixture, BM_TickScored)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        MakeScoredArchetype();
        m_registry->Reserve(static_cast<size_t>(agents), 0);

        AZStd::vector<AgentId> registered;
        registered.reserve(static_cast<size_t>(agents));
        for (int i = 0; i < agents; ++i)
        {
            registered.push_back(
                m_registry->Register(AZ::EntityId(static_cast<AZ::u64>(i) + 1), m_archetype, 0, AZ::Name{}));
        }

        // Something for every choice to argue from, so scoring reaches the end of its work
        // rather than ruling everything out on the first number it reads.
        for (const AgentId agent : registered)
        {
            for (const BlackboardKey key : m_scored)
            {
                m_blackboard->Set<float>(key, 0.5f, agent);
            }
        }

        m_registry->TickBand(0);
        m_registry->TickBand(0);

        size_t running = 0;
        for (const AgentId agent : registered)
        {
            const AgentRecord* record = m_registry->Find(agent);
            running += record != nullptr && record->m_machine.HasPlan() ? 1 : 0;
        }
        if (running != static_cast<size_t>(agents))
        {
            state.SkipWithError("agents are not running anything, so this measures an idle tick");
            return;
        }

        // An agent whose plan is merely running is reached without scoring anything at all, so
        // being busy proves nothing on its own. Move a number only the leading choice reads: if
        // the agent leaves it, the tick being measured really is re-scoring every choice.
        const AgentId probe = registered.front();
        const AZ::u16 before = ChoiceOf(probe);
        m_blackboard->Set<float>(m_scored.front(), 0.01f, probe);
        m_registry->TickBand(0);
        m_registry->TickBand(0);
        if (ChoiceOf(probe) == before)
        {
            state.SkipWithError("scoring never ran, so this measures a tick that decides nothing");
            return;
        }
        m_blackboard->Set<float>(m_scored.front(), 0.5f, probe);
        m_registry->TickBand(0);

        for ([[maybe_unused]] auto _ : state)
        {
            m_registry->TickBand(0);
        }

        state.SetItemsProcessed(state.iterations() * agents);
    }
    BENCHMARK_REGISTER_F(AgentRegistryBenchmarkFixture, BM_TickScored)->Arg(100)->Arg(1000)->Arg(10000);

    BENCHMARK_REGISTER_F(AgentRegistryBenchmarkFixture, BM_TickBand)->Arg(100)->Arg(1000)->Arg(10000);

    //! The same population, but every agent said when it next has something to do. This is what
    //! the tick costs once an action stops being asked every time whether anything changed.
    BENCHMARK_DEFINE_F(AgentRegistryBenchmarkFixture, BM_TickWaiting)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        MakeArchetype(true);
        m_registry->Reserve(static_cast<size_t>(agents), 0);

        AZStd::vector<AgentId> registered;
        registered.reserve(static_cast<size_t>(agents));
        for (int i = 0; i < agents; ++i)
        {
            registered.push_back(
                m_registry->Register(AZ::EntityId(static_cast<AZ::u64>(i) + 1), m_archetype, 0, AZ::Name{}));
        }

        for (const AgentId agent : registered)
        {
            m_blackboard->Set<bool>(m_gate, true, agent);
        }

        m_registry->TickBand(0);
        m_registry->TickBand(0);

        size_t running = 0;
        for (const AgentId agent : registered)
        {
            const AgentRecord* record = m_registry->Find(agent);
            running += record != nullptr && record->m_machine.HasPlan() ? 1 : 0;
        }
        if (running != static_cast<size_t>(agents))
        {
            state.SkipWithError("agents are not running anything, so this measures an idle tick");
            return;
        }

        for ([[maybe_unused]] auto _ : state)
        {
            m_registry->TickBand(0);
        }

        state.SetItemsProcessed(state.iterations() * agents);
    }

    BENCHMARK_REGISTER_F(AgentRegistryBenchmarkFixture, BM_TickWaiting)->Arg(100)->Arg(1000)->Arg(10000);

    //! One global write against a population that reacts to it. This is what reactivity by
    //! default costs: every agent watching the global scope wakes and rescans its guards.
    BENCHMARK_DEFINE_F(AgentRegistryBenchmarkFixture, BM_GlobalWriteReactive)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        MakeReactiveArchetype();
        m_registry->Reserve(static_cast<size_t>(agents), 0);

        AZStd::vector<AgentId> registered;
        registered.reserve(static_cast<size_t>(agents));
        for (int i = 0; i < agents; ++i)
        {
            registered.push_back(
                m_registry->Register(AZ::EntityId(static_cast<AZ::u64>(i) + 1), m_archetype, 0, AZ::Name{}));
        }

        for (const AgentId agent : registered)
        {
            m_blackboard->Set<bool>(m_gate, true, agent);
        }

        m_registry->TickBand(0);
        m_registry->TickBand(0);

        bool alarm = false;
        for ([[maybe_unused]] auto _ : state)
        {
            alarm = !alarm;
            m_blackboard->Set<bool>(m_alarm, alarm);
            m_registry->TickBand(0);
        }

        state.SetItemsProcessed(state.iterations() * agents);
    }

    BENCHMARK_REGISTER_F(AgentRegistryBenchmarkFixture, BM_GlobalWriteReactive)->Arg(100)->Arg(1000)->Arg(10000);

    //! The same, told up front how many are coming.
    BENCHMARK_DEFINE_F(AgentRegistryBenchmarkFixture, BM_RegisterAgentsReserved)(::benchmark::State& state)
    {
        const int agents = static_cast<int>(state.range(0));
        for ([[maybe_unused]] auto _ : state)
        {
            state.PauseTiming();
            Teardown();
            Build();
            MakeArchetype();
            state.ResumeTiming();

            m_registry->Reserve(static_cast<size_t>(agents), 0);
            for (int i = 0; i < agents; ++i)
            {
                m_registry->Register(AZ::EntityId(static_cast<AZ::u64>(i) + 1), m_archetype, 0, AZ::Name{});
            }
        }

        state.SetItemsProcessed(state.iterations() * agents);
    }

    BENCHMARK_REGISTER_F(AgentRegistryBenchmarkFixture, BM_RegisterAgentsReserved)->Arg(100)->Arg(1000)->Arg(10000);
} // namespace GOAT::Benchmark

#endif // HAVE_BENCHMARK
