#include <Core/Application/BlackboardSystem.h>
#include <Core/Frontend/DecisionCursor.h>
#include <Core/Frontend/TreeWalker.h>

#include <GOAT/Domain/DecisionProgram.h>
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
            context.m_planStore = &m_planStore;
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
        PlanStore m_planStore;
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

    //! One write to a slot every agent's tree observes. Today this is O(agents); it should not be.
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
} // namespace GOAT::Benchmark

#endif // HAVE_BENCHMARK
