#include <Core/Application/BlackboardSystem.h>
#include <Backends/BehaviorTree/DecisionCursor.h>
#include <Backends/BehaviorTree/TreeWalker.h>

#include <GOAT/Domain/DecisionProgram.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    //! Walks hand built programs. Nothing here loads an asset, compiles Lua or registers an
    //! entity: DecisionProgram is a plain struct, so a test can state the exact tree it means
    //! and the walk is then the only thing under test.
    class TreeWalkerFixture : public UnitTest::LeakDetectionFixture
    {
    protected:
        void SetUp() override
        {
            UnitTest::LeakDetectionFixture::SetUp();
            AZ::NameDictionary::Create();

            m_blackboard = AZStd::make_unique<BlackboardSystem>();
            m_agent = AgentId(0, 1);
            m_blackboard->CreateAgentBlackboard(m_agent);
        }

        void TearDown() override
        {
            m_blackboard.reset();
            AZ::NameDictionary::Destroy();
            UnitTest::LeakDetectionFixture::TearDown();
        }

        PlanContext Context()
        {
            PlanContext context;
            context.m_agent = m_agent;
            context.m_blackboard = m_blackboard.get();
            return context;
        }

        //! Declares an agent scoped bool and gives it a value.
        BlackboardKey DeclareBool(const char* name, bool value)
        {
            const auto declared = m_blackboard->Declare(AZ::Name(name), BlackboardScope::Agent, BlackboardType::Bool);
            EXPECT_TRUE(declared.IsSuccess());
            const BlackboardKey key = declared.GetValue();
            m_blackboard->Set<bool>(key, value, m_agent);
            return key;
        }

        //! Appends a node, returning its index. Callers fix up the tree links afterwards, which
        //! keeps each test's tree shape visible in the test rather than hidden in a helper.
        static NodeIndex Add(DecisionProgram& program, NodeOp op, NodeIndex parent)
        {
            const NodeIndex index = static_cast<NodeIndex>(program.m_nodes.size());
            DecisionNode node;
            node.m_op = op;
            node.m_parent = parent;
            program.m_nodes.push_back(node);
            return index;
        }

        //! Closes every subtree by pre-order extent, which is the invariant the walker relies on.
        static void Finish(DecisionProgram& program)
        {
            const NodeIndex count = static_cast<NodeIndex>(program.m_nodes.size());
            for (NodeIndex i = count; i > 0; --i)
            {
                DecisionNode& node = program.m_nodes[i - 1];
                node.m_subtreeEnd = i;
                node.m_childCount = 0;
                node.m_firstChild = InvalidNodeIndex;
            }

            // A parent's extent covers every descendant, and its first child follows it directly.
            for (NodeIndex i = count; i > 0; --i)
            {
                DecisionNode& node = program.m_nodes[i - 1];
                if (node.m_parent == InvalidNodeIndex)
                {
                    continue;
                }

                DecisionNode& parent = program.m_nodes[node.m_parent];
                parent.m_subtreeEnd = AZStd::max(parent.m_subtreeEnd, node.m_subtreeEnd);
                parent.m_childCount++;
                if (parent.m_firstChild == InvalidNodeIndex || (i - 1) < parent.m_firstChild)
                {
                    parent.m_firstChild = i - 1;
                }
            }
        }

        AgentId m_agent;
        AZStd::unique_ptr<BlackboardSystem> m_blackboard;
        TreeWalker m_walker;
    };

    //! A selector whose first branch is closed picks the second.
    TEST_F(TreeWalkerFixture, Selector_SkipsAFailingConditionAndTakesTheNextBranch)
    {
        const BlackboardKey closed = DeclareBool("closed", false);

        DecisionProgram program;
        program.m_name = AZ::Name("Test");
        const NodeIndex root = Add(program, NodeOp::Selector, InvalidNodeIndex);
        const NodeIndex guard = Add(program, NodeOp::Condition, root);
        const NodeIndex second = Add(program, NodeOp::Action, root);
        Finish(program);

        program.m_nodes[guard].m_key = closed;
        program.m_nodes[second].m_action.m_amount = 42.0f;

        DecisionCursor cursor;
        cursor.Reset(program);

        const WalkStep step = m_walker.Begin(program, cursor, Context());

        EXPECT_EQ(step.m_outcome, WalkOutcome::Intent);
        EXPECT_EQ(cursor.GetActiveLeaf(), second);
    }

    //! The same tree with the condition open stops at the condition's branch instead.
    TEST_F(TreeWalkerFixture, Selector_TakesTheFirstBranchWhenItsConditionHolds)
    {
        const BlackboardKey open = DeclareBool("open", true);

        DecisionProgram program;
        program.m_name = AZ::Name("Test");
        const NodeIndex root = Add(program, NodeOp::Sequence, InvalidNodeIndex);
        const NodeIndex guard = Add(program, NodeOp::Condition, root);
        const NodeIndex body = Add(program, NodeOp::Action, root);
        Finish(program);

        program.m_nodes[guard].m_key = open;

        DecisionCursor cursor;
        cursor.Reset(program);

        const WalkStep step = m_walker.Begin(program, cursor, Context());

        EXPECT_EQ(step.m_outcome, WalkOutcome::Intent);
        EXPECT_EQ(cursor.GetActiveLeaf(), body);
    }

    //! A sequence whose guard is closed produces no work at all. This is the shape that made
    //! idle agents expensive, so it is worth stating as its own expectation.
    TEST_F(TreeWalkerFixture, Sequence_FinishesWithNoIntentWhenItsGuardIsClosed)
    {
        const BlackboardKey closed = DeclareBool("closed", false);

        DecisionProgram program;
        program.m_name = AZ::Name("Test");
        const NodeIndex root = Add(program, NodeOp::Sequence, InvalidNodeIndex);
        const NodeIndex guard = Add(program, NodeOp::Condition, root);
        Add(program, NodeOp::Action, root);
        Finish(program);

        program.m_nodes[guard].m_key = closed;

        DecisionCursor cursor;
        cursor.Reset(program);

        const WalkStep step = m_walker.Begin(program, cursor, Context());

        EXPECT_EQ(step.m_outcome, WalkOutcome::Finished);
        EXPECT_EQ(cursor.GetActiveLeaf(), InvalidNodeIndex);
    }

    //! Walking a finished tree twice in a row must produce the same answer, because nothing
    //! between the two walks can change what the predicates read. This is the property that
    //! makes the second Begin call in AgentRuntime redundant.
    TEST_F(TreeWalkerFixture, Begin_IsRepeatableWhenNothingChanged)
    {
        const BlackboardKey closed = DeclareBool("closed", false);

        DecisionProgram program;
        program.m_name = AZ::Name("Test");
        const NodeIndex root = Add(program, NodeOp::Sequence, InvalidNodeIndex);
        const NodeIndex guard = Add(program, NodeOp::Condition, root);
        Add(program, NodeOp::Action, root);
        Finish(program);

        program.m_nodes[guard].m_key = closed;

        DecisionCursor cursor;
        cursor.Reset(program);

        const WalkStep first = m_walker.Begin(program, cursor, Context());
        const WalkStep second = m_walker.Begin(program, cursor, Context());

        EXPECT_EQ(first.m_outcome, WalkOutcome::Finished);
        EXPECT_EQ(second.m_outcome, first.m_outcome);
        EXPECT_EQ(second.m_result, first.m_result);
    }

    //! Flipping the guard is what a walk is waiting for, and it must change the answer.
    TEST_F(TreeWalkerFixture, Begin_ProducesWorkOnceTheGuardOpens)
    {
        const BlackboardKey gate = DeclareBool("gate", false);

        DecisionProgram program;
        program.m_name = AZ::Name("Test");
        const NodeIndex root = Add(program, NodeOp::Sequence, InvalidNodeIndex);
        const NodeIndex guard = Add(program, NodeOp::Condition, root);
        const NodeIndex body = Add(program, NodeOp::Action, root);
        Finish(program);

        program.m_nodes[guard].m_key = gate;

        DecisionCursor cursor;
        cursor.Reset(program);

        EXPECT_EQ(m_walker.Begin(program, cursor, Context()).m_outcome, WalkOutcome::Finished);

        m_blackboard->Set<bool>(gate, true, m_agent);

        const WalkStep after = m_walker.Begin(program, cursor, Context());
        EXPECT_EQ(after.m_outcome, WalkOutcome::Intent);
        EXPECT_EQ(cursor.GetActiveLeaf(), body);
    }
} // namespace GOAT
