#include <TestAgentSystem.h>

#include <Tools/GraphEditor/Core.h>
#include <Tools/GraphEditor/GraphContext.h>
#include <Tools/GraphEditor/ProgramGraphSerializer.h>
#include <Tools/GraphEditor/ProgramLayout.h>
#include <Tools/GraphEditor/ProgramNode.h>
#include <Tools/GraphEditor/ProgramValidator.h>

#include <GraphModel/Model/Graph.h>
#include <GraphModel/Model/Slot.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/Name/NameDictionary.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzTest/AzTest.h>

namespace GOAT
{
    using namespace GOAT::GraphEditor;

    //! A registry holding a handful of words, so the editor is exercised against the same
    //! shape of descriptor a backend registers rather than against the real vocabulary.
    class ProgramGraphFixture
        : public UnitTest::LeakDetectionFixture
    {
    public:
        void SetUp() override
        {
            UnitTest::LeakDetectionFixture::SetUp();
            AZ::NameDictionary::Create();

            // Built after the dictionary exists: the registry names its built-in words as it
            // is constructed, and a member would be constructed before SetUp ran.
            m_nodeTypes = AZStd::make_unique<NodeTypeRegistry>();
            m_actions = AZStd::make_unique<ActionStateRegistry>();

            NodeTypeDescriptor sequence;
            sequence.m_name = AZ::Name("sequence");
            sequence.m_kind = NodeKind::Composite;
            sequence.m_op = NodeOp::Sequence;
            sequence.m_category = "Composite";
            sequence.m_backend = AZ::Name("tree");
            m_nodeTypes->Register(sequence);

            NodeTypeDescriptor wait;
            wait.m_name = AZ::Name("wait");
            wait.m_kind = NodeKind::Leaf;
            wait.m_op = NodeOp::Action;
            wait.m_category = "Leaf";
            NodeParameter seconds;
            seconds.m_name = AZ::Name("seconds");
            seconds.m_type = BlackboardType::Float;
            seconds.m_required = true;
            wait.m_parameters.push_back(seconds);
            m_nodeTypes->Register(wait);

            NodeTypeDescriptor invert;
            invert.m_name = AZ::Name("invert");
            invert.m_kind = NodeKind::Decorator;
            invert.m_op = NodeOp::Invert;
            invert.m_category = "Decorator";
            invert.m_backend = AZ::Name("tree");
            m_nodeTypes->Register(invert);

            NodeTypeDescriptor watch;
            watch.m_name = AZ::Name("watch");
            watch.m_kind = NodeKind::Service;
            watch.m_category = "Service";
            watch.m_backend = AZ::Name("tree");
            m_nodeTypes->Register(watch);

            NodeTypeDescriptor task;
            task.m_name = AZ::Name("task");
            task.m_kind = NodeKind::Composite;
            task.m_category = "Task Network";
            task.m_backend = AZ::Name("htn");
            m_nodeTypes->Register(task);

            m_agents = AZStd::make_unique<TestAgentSystem>(*m_nodeTypes, *m_actions);
            AZ::Interface<IAgentSystem>::Register(m_agents.get());

            m_context = AZStd::make_shared<GraphContext>();
        }

        void TearDown() override
        {
            m_context.reset();
            AZ::Interface<IAgentSystem>::Unregister(m_agents.get());
            m_agents.reset();
            m_actions.reset();
            m_nodeTypes.reset();
            AZ::NameDictionary::Destroy();
            UnitTest::LeakDetectionFixture::TearDown();
        }

        //! A sequence of two waits, the second one placed above the first.
        static AuthoredNode TwoWaits()
        {
            AuthoredNode root;
            root.m_type = "sequence";
            root.m_metadata.m_position = AZ::Vector2(0.0f, 100.0f);

            AuthoredNode first;
            first.m_type = "wait";
            first.m_metadata.m_position = AZ::Vector2(260.0f, 0.0f);
            AuthoredProperty seconds;
            seconds.m_name = "seconds";
            seconds.m_value = AZStd::any(1.5f);
            first.m_properties.push_back(seconds);

            AuthoredNode second = first;
            second.m_metadata.m_position = AZ::Vector2(260.0f, 200.0f);
            second.m_properties[0].m_value = AZStd::any(2.5f);

            root.m_children.push_back(first);
            root.m_children.push_back(second);
            return root;
        }

        AZStd::unique_ptr<NodeTypeRegistry> m_nodeTypes;
        AZStd::unique_ptr<ActionStateRegistry> m_actions;
        AZStd::unique_ptr<TestAgentSystem> m_agents;
        AZStd::shared_ptr<GraphContext> m_context;
    };

    TEST_F(ProgramGraphFixture, ANodeTakesItsSlotsFromTheRegistry)
    {
        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);
        auto node = AZStd::make_shared<ProgramNode>(graph, "wait");

        EXPECT_NE(node->GetSlot(ParentSlotId), nullptr);
        EXPECT_EQ(node->GetSlot(ChildrenSlotId), nullptr) << "a leaf cannot be given a child";
        EXPECT_NE(node->GetSlot(ProgramNode::PropertySlotId("seconds")), nullptr);
    }

    TEST_F(ProgramGraphFixture, ANodeIsTitledWithItsWordAndCategory)
    {
        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);
        auto node = AZStd::make_shared<ProgramNode>(graph, "sequence");

        EXPECT_STREQ(node->GetTitle(), "sequence");
        EXPECT_STREQ(node->GetSubTitle(), "Composite");
    }

    TEST_F(ProgramGraphFixture, OneChildSlotTakesEveryChild)
    {
        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);
        auto root = AZStd::make_shared<ProgramNode>(graph, "sequence");
        graph->AddNode(root);

        GraphModel::SlotPtr children = root->GetSlot(ChildrenSlotId);
        ASSERT_NE(children, nullptr);
        EXPECT_FALSE(children->SupportsExtendability())
            << "an extendable slot would make the author add one slot per child";

        for (int i = 0; i < 3; ++i)
        {
            auto child = AZStd::make_shared<ProgramNode>(graph, "wait");
            graph->AddNode(child);
            EXPECT_NE(graph->AddConnection(children, child->GetSlot(ParentSlotId)), nullptr);
        }
        EXPECT_EQ(children->GetConnections().size(), 3u);

        graph->ClearCachedData();
    }

    TEST_F(ProgramGraphFixture, ACompositeHoldsChildrenAndServices)
    {
        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);
        auto node = AZStd::make_shared<ProgramNode>(graph, "sequence");

        EXPECT_NE(node->GetSlot(ChildrenSlotId), nullptr);
        EXPECT_NE(node->GetSlot(ServicesSlotId), nullptr);
    }

    TEST_F(ProgramGraphFixture, ADecoratorHoldsChildrenButNoServices)
    {
        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);
        auto node = AZStd::make_shared<ProgramNode>(graph, "invert");

        EXPECT_NE(node->GetSlot(ChildrenSlotId), nullptr);
        EXPECT_EQ(node->GetSlot(ServicesSlotId), nullptr);
    }

    TEST_F(ProgramGraphFixture, AProgramWithoutPositionsIsLaidOutRatherThanTrusted)
    {
        AuthoredNode lua = TwoWaits();
        lua.m_metadata.m_position = AZ::Vector2::CreateZero();
        lua.m_children[0].m_metadata.m_position = AZ::Vector2::CreateZero();
        lua.m_children[1].m_metadata.m_position = AZ::Vector2::CreateZero();
        EXPECT_FALSE(HasAuthoredLayout(lua)) << "nothing in it says where anything sits";

        // One placed node anywhere in the tree is enough to call the layout the author's.
        lua.m_children[1].m_metadata.m_position = AZ::Vector2(10.0f, 20.0f);
        EXPECT_TRUE(HasAuthoredLayout(lua));

        EXPECT_TRUE(HasAuthoredLayout(TwoWaits()));
    }

    TEST_F(ProgramGraphFixture, FlatteningKeepsTheAuthoredTreeShape)
    {
        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);
        const AZStd::vector<PlacedNode> placed = FromAuthored(TwoWaits(), graph);

        ASSERT_EQ(placed.size(), 3u);
        EXPECT_EQ(placed[0].m_parent, -1);
        EXPECT_EQ(placed[1].m_parent, 0);
        EXPECT_EQ(placed[2].m_parent, 0);
        EXPECT_EQ(placed[0].m_position, AZ::Vector2(0.0f, 100.0f)) << "an authored position is kept";
    }

    TEST_F(ProgramGraphFixture, AnUnplacedTreeIsLaidOutLeftToRight)
    {
        AuthoredNode root = TwoWaits();
        root.m_metadata.m_position = AZ::Vector2::CreateZero();
        root.m_children[0].m_metadata.m_position = AZ::Vector2::CreateZero();
        root.m_children[1].m_metadata.m_position = AZ::Vector2::CreateZero();

        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);
        const AZStd::vector<PlacedNode> placed = FromAuthored(root, graph);

        ASSERT_EQ(placed.size(), 3u);
        EXPECT_LT(placed[0].m_position.GetX(), placed[1].m_position.GetX())
            << "a child sits to the right of its parent";
        EXPECT_LT(placed[1].m_position.GetY(), placed[2].m_position.GetY())
            << "the first child sits above the second";
    }

    TEST_F(ProgramGraphFixture, ChildOrderFollowsPositionRatherThanInsertionOrder)
    {
        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);

        auto root = AZStd::make_shared<ProgramNode>(graph, "sequence");
        auto first = AZStd::make_shared<ProgramNode>(graph, "wait");
        auto second = AZStd::make_shared<ProgramNode>(graph, "wait");
        graph->AddNode(root);
        graph->AddNode(first);
        graph->AddNode(second);

        first->GetSlot(ProgramNode::PropertySlotId("seconds"))->SetValue(1.0f);
        second->GetSlot(ProgramNode::PropertySlotId("seconds"))->SetValue(2.0f);

        // Connected first-then-second, but laid out second-above-first.
        graph->AddConnection(root->GetSlot(ChildrenSlotId), first->GetSlot(ParentSlotId));
        graph->AddConnection(root->GetSlot(ChildrenSlotId), second->GetSlot(ParentSlotId));

        AZStd::unordered_map<const GraphModel::Node*, float> rows{
            { root.get(), 0.0f }, { first.get(), 200.0f }, { second.get(), 100.0f }
        };
        auto positionOf = [&rows](GraphModel::ConstNodePtr node)
        {
            return AZ::Vector2(0.0f, rows[node.get()]);
        };

        auto authored = ToAuthored(graph, positionOf);
        ASSERT_TRUE(authored.IsSuccess()) << authored.GetError().c_str();

        const AuthoredNode& read = authored.GetValue();
        ASSERT_EQ(read.m_children.size(), 2u);
        ASSERT_EQ(read.m_children[0].m_properties.size(), 1u);
        EXPECT_FLOAT_EQ(AZStd::any_cast<float>(read.m_children[0].m_properties[0].m_value), 2.0f)
            << "the higher node runs first, whatever order it was connected in";
        EXPECT_FLOAT_EQ(AZStd::any_cast<float>(read.m_children[1].m_properties[0].m_value), 1.0f);

        graph->ClearCachedData();
    }

    TEST_F(ProgramGraphFixture, APathTellsAServiceApartFromAChild)
    {
        // A composite carrying both. Before services and children shared one index space,
        // path {0} named the service and the first child alike, and whichever the reader
        // reached first won.
        AuthoredNode root;
        root.m_type = "sequence";

        AuthoredNode service;
        service.m_type = "watch";
        root.m_services.push_back(service);

        AuthoredNode broken;
        broken.m_type = "wait";
        root.m_children.push_back(broken);

        const ValidationResult result = Validate(root, AZ::Name("Test"));
        EXPECT_FALSE(result.m_valid);
        EXPECT_NE(result.m_error.find("requires property 'seconds'"), AZStd::string::npos)
            << result.m_error.c_str();
        ASSERT_EQ(result.m_path.size(), 1u);
        EXPECT_EQ(result.m_path[0], 1u) << "the child sits after the service, not on top of it";
    }

    TEST_F(ProgramGraphFixture, APathResolvesServicesBeforeChildrenOnTheCanvas)
    {
        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);

        auto root = AZStd::make_shared<ProgramNode>(graph, "sequence");
        auto service = AZStd::make_shared<ProgramNode>(graph, "watch");
        auto child = AZStd::make_shared<ProgramNode>(graph, "wait");
        graph->AddNode(root);
        graph->AddNode(service);
        graph->AddNode(child);

        graph->AddConnection(root->GetSlot(ServicesSlotId), service->GetSlot(ParentSlotId));
        graph->AddConnection(root->GetSlot(ChildrenSlotId), child->GetSlot(ParentSlotId));

        // The service is drawn *below* the child, so anything sorting the two slots together
        // would put the child first and hand back the wrong node for both paths.
        AZStd::unordered_map<const GraphModel::Node*, float> rows{
            { root.get(), 0.0f }, { service.get(), 300.0f }, { child.get(), 100.0f }
        };
        auto positionOf = [&rows](GraphModel::ConstNodePtr node)
        {
            return AZ::Vector2(0.0f, rows[node.get()]);
        };

        EXPECT_EQ(NodeAtPath(graph, { 0 }, positionOf), service) << "index 0 is the service";
        EXPECT_EQ(NodeAtPath(graph, { 1 }, positionOf), child) << "children follow the services";
        EXPECT_EQ(NodeAtPath(graph, { 2 }, positionOf), nullptr) << "a path off the end leads nowhere";
        EXPECT_EQ(NodeAtPath(graph, {}, positionOf), root) << "an empty path is the root";

        graph->ClearCachedData();
    }

    TEST_F(ProgramGraphFixture, AValidatorPathFindsTheNodeItAccuses)
    {
        // The two halves of the contract checked against each other rather than against a
        // number written out by hand: what the validator blames, the canvas must find.
        // The fault is a decorator given two children, because that is one an author can
        // actually draw -- a required property read back off a slot carries the type's
        // default rather than going missing.
        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);

        auto root = AZStd::make_shared<ProgramNode>(graph, "sequence");
        auto service = AZStd::make_shared<ProgramNode>(graph, "watch");
        auto good = AZStd::make_shared<ProgramNode>(graph, "wait");
        auto crowded = AZStd::make_shared<ProgramNode>(graph, "invert");
        auto one = AZStd::make_shared<ProgramNode>(graph, "wait");
        auto two = AZStd::make_shared<ProgramNode>(graph, "wait");
        for (const auto& node : { root, service, good, crowded, one, two })
        {
            graph->AddNode(node);
        }

        graph->AddConnection(root->GetSlot(ServicesSlotId), service->GetSlot(ParentSlotId));
        graph->AddConnection(root->GetSlot(ChildrenSlotId), good->GetSlot(ParentSlotId));
        graph->AddConnection(root->GetSlot(ChildrenSlotId), crowded->GetSlot(ParentSlotId));
        graph->AddConnection(crowded->GetSlot(ChildrenSlotId), one->GetSlot(ParentSlotId));
        graph->AddConnection(crowded->GetSlot(ChildrenSlotId), two->GetSlot(ParentSlotId));

        // The service is drawn below everything, so a lookup that sorted the two slots
        // together would shift every child by one and accuse the wrong node.
        AZStd::unordered_map<const GraphModel::Node*, float> rows{
            { root.get(), 0.0f }, { service.get(), 900.0f }, { good.get(), 100.0f },
            { crowded.get(), 200.0f }, { one.get(), 300.0f }, { two.get(), 400.0f }
        };
        auto positionOf = [&rows](GraphModel::ConstNodePtr node)
        {
            return AZ::Vector2(0.0f, rows[node.get()]);
        };

        auto authored = ToAuthored(graph, positionOf);
        ASSERT_TRUE(authored.IsSuccess()) << authored.GetError().c_str();

        const ValidationResult result = Validate(authored.GetValue(), AZ::Name("Test"));
        ASSERT_FALSE(result.m_valid);
        EXPECT_NE(result.m_error.find("cannot have 2 children"), AZStd::string::npos)
            << result.m_error.c_str();
        ASSERT_EQ(result.m_path.size(), 1u);
        EXPECT_EQ(result.m_path[0], 2u) << "one service, then the second child";
        EXPECT_EQ(NodeAtPath(graph, result.m_path, positionOf), crowded)
            << "the path the validator wrote leads to the node it complained about";

        graph->ClearCachedData();
    }

    TEST_F(ProgramGraphFixture, AGraphWithNoRootIsRefused)
    {
        auto graph = AZStd::make_shared<GraphModel::Graph>(m_context);
        auto lonely = AZStd::make_shared<ProgramNode>(graph, "sequence");
        auto other = AZStd::make_shared<ProgramNode>(graph, "sequence");
        graph->AddNode(lonely);
        graph->AddNode(other);

        auto authored = ToAuthored(graph, [](GraphModel::ConstNodePtr) { return AZ::Vector2::CreateZero(); });
        EXPECT_FALSE(authored.IsSuccess()) << "two nodes run under nothing, so there is no one root";

        graph->ClearCachedData();
    }

    TEST_F(ProgramGraphFixture, SiblingsAreLaidOutInOrderAndNeverOverlap)
    {
        // A root with three children of quite different heights, as measured on a canvas.
        const AZStd::vector<LayoutNode> nodes{
            { -1, 200.0f, 120.0f }, { 0, 80.0f, 300.0f }, { 0, 80.0f, 60.0f }, { 0, 80.0f, 140.0f }
        };

        const AZStd::vector<AZ::Vector2> at = LayoutProgram(nodes);
        ASSERT_EQ(at.size(), 4u);

        EXPECT_LT(at[0].GetX(), at[1].GetX()) << "a child sits to the right of its parent";
        EXPECT_FLOAT_EQ(at[1].GetX(), at[2].GetX());
        EXPECT_FLOAT_EQ(at[2].GetX(), at[3].GetX());

        // The child that runs first is the one that sits highest.
        EXPECT_LT(at[1].GetY(), at[2].GetY());
        EXPECT_LT(at[2].GetY(), at[3].GetY());

        // And no sibling may run into the one after it, whatever their heights are.
        for (size_t i = 1; i + 1 < nodes.size(); ++i)
        {
            EXPECT_GE(at[i + 1].GetY(), at[i].GetY() + nodes[i].m_height)
                << "child " << i << " runs into child " << (i + 1);
        }

        // The parent sits level with the block it owns.
        const float blockTop = at[1].GetY();
        const float blockBottom = at[3].GetY() + nodes[3].m_height;
        EXPECT_NEAR(at[0].GetY() + nodes[0].m_height * 0.5f, (blockTop + blockBottom) * 0.5f, 0.01f);
    }

    TEST_F(ProgramGraphFixture, ADeeperColumnClearsTheWidestNodeBeforeIt)
    {
        const AZStd::vector<LayoutNode> nodes{
            { -1, 400.0f, 100.0f }, { 0, 80.0f, 100.0f }, { 1, 80.0f, 100.0f }
        };

        const AZStd::vector<AZ::Vector2> at = LayoutProgram(nodes);
        ASSERT_EQ(at.size(), 3u);
        EXPECT_GE(at[1].GetX(), at[0].GetX() + nodes[0].m_width)
            << "a wide parent must not run into the column after it";
        EXPECT_GE(at[2].GetX(), at[1].GetX() + nodes[1].m_width);
    }

    TEST_F(ProgramGraphFixture, ValidationReportsAMissingRequiredProperty)
    {
        AuthoredNode root;
        root.m_type = "sequence";
        AuthoredNode wait;
        wait.m_type = "wait";
        root.m_children.push_back(wait);

        const ValidationResult result = Validate(root, AZ::Name("Test"));
        EXPECT_FALSE(result.m_valid);
        EXPECT_NE(result.m_error.find("requires property 'seconds'"), AZStd::string::npos)
            << result.m_error.c_str();
        ASSERT_EQ(result.m_path.size(), 1u);
        EXPECT_EQ(result.m_path[0], 0u) << "the fault is pointed at the child that carries it";
    }

    TEST_F(ProgramGraphFixture, ValidationRefusesAWordAnotherBackendOwns)
    {
        AuthoredNode root;
        root.m_type = "sequence";
        AuthoredNode task;
        task.m_type = "task";
        root.m_children.push_back(task);

        const ValidationResult result = Validate(root, AZ::Name("Test"));
        EXPECT_FALSE(result.m_valid);
        EXPECT_NE(result.m_error.find("belongs to the 'htn' backend"), AZStd::string::npos)
            << result.m_error.c_str();
    }

    TEST_F(ProgramGraphFixture, ValidationRefusesAnUnknownWord)
    {
        AuthoredNode root;
        root.m_type = "nonsense";

        const ValidationResult result = Validate(root, AZ::Name("Test"));
        EXPECT_FALSE(result.m_valid);
        EXPECT_NE(result.m_error.find("not a word any backend registered"), AZStd::string::npos)
            << result.m_error.c_str();
    }

    TEST_F(ProgramGraphFixture, ADecoratorMustHaveExactlyOneChild)
    {
        AuthoredNode root;
        root.m_type = "invert";

        const ValidationResult result = Validate(root, AZ::Name("Test"));
        EXPECT_FALSE(result.m_valid);
        EXPECT_NE(result.m_error.find("cannot have 0 children"), AZStd::string::npos)
            << result.m_error.c_str();
    }

    TEST_F(ProgramGraphFixture, TheOwningBackendComesFromTheRootWord)
    {
        AuthoredNode tree;
        tree.m_type = "sequence";
        EXPECT_EQ(FindOwningBackend(tree), AZ::Name("tree"));

        AuthoredNode domain;
        domain.m_type = "task";
        EXPECT_EQ(FindOwningBackend(domain), AZ::Name("htn"));

        // A core word belongs to no paradigm, and a behaviour tree is what runs one.
        AuthoredNode leaf;
        leaf.m_type = "wait";
        EXPECT_EQ(FindOwningBackend(leaf), AZ::Name("tree"));
    }

    TEST_F(ProgramGraphFixture, AProgramSurvivesBeingWrittenAndReadBack)
    {
        AZ::SerializeContext serialize;
        AZ::Data::AssetData::Reflect(&serialize);
        ProgramAsset::Reflect(&serialize);

        ProgramAsset written;
        written.m_name = "Guard";
        written.m_root = TwoWaits();
        written.m_root.m_metadata.m_comment = "the tree a guard runs";

        // The same XML a .goat file holds, without needing a file system under the test.
        AZStd::vector<AZ::u8> buffer;
        AZ::IO::ByteContainerStream<decltype(buffer)> stream(&buffer);
        ASSERT_TRUE(AZ::Utils::SaveObjectToStream(stream, AZ::ObjectStream::ST_XML, &written, &serialize));

        ProgramAsset read;
        ASSERT_TRUE(AZ::Utils::LoadObjectFromBufferInPlace(buffer.data(), buffer.size(), read, &serialize));

        EXPECT_EQ(read.m_name, "Guard");
        EXPECT_EQ(read.m_root.m_type, "sequence");
        EXPECT_EQ(read.m_root.m_metadata.m_comment, "the tree a guard runs");
        EXPECT_EQ(read.m_root.m_metadata.m_position, AZ::Vector2(0.0f, 100.0f));

        ASSERT_EQ(read.m_root.m_children.size(), 2u);
        ASSERT_EQ(read.m_root.m_children[0].m_properties.size(), 1u);
        EXPECT_EQ(read.m_root.m_children[0].m_properties[0].m_name, "seconds");
        EXPECT_FLOAT_EQ(
            AZStd::any_cast<float>(read.m_root.m_children[0].m_properties[0].m_value), 1.5f)
            << "a property value survives the any round trip";
        EXPECT_EQ(read.m_root.m_children[1].m_metadata.m_position, AZ::Vector2(260.0f, 200.0f));
    }
} // namespace GOAT
