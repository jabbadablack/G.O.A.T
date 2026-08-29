#include <Tools/GraphEditor/ProgramGraphSerializer.h>
#include <Tools/GraphEditor/Core.h>

#include <GraphModel/Model/Connection.h>
#include <GraphModel/Model/Graph.h>
#include <GraphModel/Model/Slot.h>

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/smart_ptr/make_shared.h>
#include <AzCore/std/sort.h>

namespace GOAT::GraphEditor
{
    namespace
    {
        //! Spacing used when a program has never been opened in the editor before.
        constexpr float ColumnWidth = 260.0f;
        constexpr float RowHeight = 140.0f;

        struct LayoutCursor final
        {
            //! Next free row, so no two nodes are laid out on top of each other.
            float m_nextRow = 0.0f;
        };

        void Flatten(const AuthoredNode& authored, int parent, bool isService,
            GraphModel::GraphPtr graph, int depth, LayoutCursor& cursor,
            AZStd::vector<PlacedNode>& out)
        {
            PlacedNode placed;
            placed.m_node = AZStd::make_shared<ProgramNode>(graph, authored.m_type);
            placed.m_parent = parent;
            placed.m_isService = isService;
            placed.m_position = authored.m_metadata.m_position;

            const int index = static_cast<int>(out.size());
            out.push_back(AZStd::move(placed));

            // Children first, so a parent sits level with the block it owns rather than above it.
            const float firstRow = cursor.m_nextRow;
            for (const AuthoredNode& service : authored.m_services)
            {
                Flatten(service, index, true, graph, depth + 1, cursor, out);
            }
            for (const AuthoredNode& child : authored.m_children)
            {
                Flatten(child, index, false, graph, depth + 1, cursor, out);
            }

            if (authored.m_children.empty() && authored.m_services.empty())
            {
                cursor.m_nextRow += RowHeight;
            }

            if (out[index].m_position.IsZero())
            {
                const float lastRow = AZStd::max(firstRow, cursor.m_nextRow - RowHeight);
                out[index].m_position =
                    AZ::Vector2(static_cast<float>(depth) * ColumnWidth, (firstRow + lastRow) * 0.5f);
            }
        }

        //! Reads a node's properties back off its property slots, skipping anything left at the
        //! type's default so an untouched optional property is not written out as authored.
        void ReadProperties(const ProgramNode& node, AuthoredNode& out)
        {
            const NodeTypeDescriptor* descriptor = node.GetDescriptor();
            if (descriptor == nullptr)
            {
                return;
            }

            for (const NodeParameter& parameter : descriptor->m_parameters)
            {
                GraphModel::ConstSlotPtr slot =
                    node.GetSlot(ProgramNode::PropertySlotId(parameter.m_name.GetStringView()));
                if (slot == nullptr)
                {
                    continue;
                }

                const AZStd::any value = slot->GetValue();
                if (value.empty())
                {
                    continue;
                }

                if (const auto* text = AZStd::any_cast<AZStd::string>(&value); text != nullptr && text->empty())
                {
                    continue;
                }

                AuthoredProperty property;
                property.m_name = parameter.m_name.GetCStr();
                property.m_value = value;
                out.m_properties.push_back(AZStd::move(property));
            }
        }

        //! What hangs off one of a node's two structural slots, ordered by how far down the
        //! canvas each sits.
        AZStd::vector<GraphModel::NodePtr> ChildrenOf(
            GraphModel::NodePtr node, const char* slotName, const PositionLookup& positionOf)
        {
            AZStd::vector<GraphModel::NodePtr> children;

            for (GraphModel::SlotPtr slot : node->GetExtendableSlots(slotName))
            {
                for (const GraphModel::ConnectionPtr& connection : slot->GetConnections())
                {
                    if (GraphModel::NodePtr target = connection->GetTargetNode();
                        target != nullptr && target != node)
                    {
                        children.push_back(target);
                    }
                }
            }

            // A single, non extended slot is not in the extendable set, so it is read directly.
            if (children.empty())
            {
                if (GraphModel::SlotPtr slot = node->GetSlot(slotName); slot != nullptr)
                {
                    for (const GraphModel::ConnectionPtr& connection : slot->GetConnections())
                    {
                        if (GraphModel::NodePtr target = connection->GetTargetNode();
                            target != nullptr && target != node)
                        {
                            children.push_back(target);
                        }
                    }
                }
            }

            AZStd::stable_sort(children.begin(), children.end(),
                [&positionOf](const GraphModel::NodePtr& lhs, const GraphModel::NodePtr& rhs)
                {
                    return positionOf(lhs).GetY() < positionOf(rhs).GetY();
                });
            return children;
        }

        AZ::Outcome<AuthoredNode, AZStd::string> ReadNode(GraphModel::NodePtr node,
            const PositionLookup& positionOf, AZStd::unordered_set<GraphModel::Node*>& visiting)
        {
            auto* program = azrtti_cast<ProgramNode*>(node.get());
            if (program == nullptr)
            {
                return AZ::Failure(AZStd::string("The graph holds a node that is not a program node"));
            }

            if (!visiting.insert(node.get()).second)
            {
                return AZ::Failure(AZStd::string::format(
                    "'%s' runs under itself, which is a loop rather than a tree",
                    program->GetTypeName().c_str()));
            }

            AuthoredNode authored;
            authored.m_type = program->GetTypeName();
            authored.m_metadata.m_position = positionOf(node);
            ReadProperties(*program, authored);

            for (const GraphModel::NodePtr& service : ChildrenOf(node, ServicesSlotId, positionOf))
            {
                auto read = ReadNode(service, positionOf, visiting);
                if (!read.IsSuccess())
                {
                    return read;
                }
                authored.m_services.push_back(read.TakeValue());
            }

            for (const GraphModel::NodePtr& child : ChildrenOf(node, ChildrenSlotId, positionOf))
            {
                auto read = ReadNode(child, positionOf, visiting);
                if (!read.IsSuccess())
                {
                    return read;
                }
                authored.m_children.push_back(read.TakeValue());
            }

            visiting.erase(node.get());
            return AZ::Success(AZStd::move(authored));
        }
    } // namespace

    bool HasAuthoredLayout(const AuthoredNode& root)
    {
        if (!root.m_metadata.m_position.IsZero())
        {
            return true;
        }
        for (const AuthoredNode& service : root.m_services)
        {
            if (HasAuthoredLayout(service))
            {
                return true;
            }
        }
        for (const AuthoredNode& child : root.m_children)
        {
            if (HasAuthoredLayout(child))
            {
                return true;
            }
        }
        return false;
    }

    AZStd::vector<PlacedNode> FromAuthored(const AuthoredNode& root, GraphModel::GraphPtr graph)
    {
        AZStd::vector<PlacedNode> placed;
        LayoutCursor cursor;
        Flatten(root, -1, false, graph, 0, cursor, placed);
        return placed;
    }

    AZ::Outcome<AuthoredNode, AZStd::string> ToAuthored(
        GraphModel::GraphPtr graph, const PositionLookup& positionOf)
    {
        if (graph == nullptr)
        {
            return AZ::Failure(AZStd::string("There is no graph to read"));
        }

        // The root is the one node nothing runs above.
        AZStd::vector<GraphModel::NodePtr> roots;
        for (const auto& [id, node] : graph->GetNodes())
        {
            GraphModel::SlotPtr parent = node->GetSlot(ParentSlotId);
            if (parent == nullptr || parent->GetConnections().empty())
            {
                roots.push_back(node);
            }
        }

        if (roots.empty())
        {
            return AZ::Failure(AZStd::string("Every node runs under another, so the program has no root"));
        }
        if (roots.size() > 1)
        {
            return AZ::Failure(AZStd::string::format(
                "%zu nodes run under nothing; a program has one root", roots.size()));
        }

        AZStd::unordered_set<GraphModel::Node*> visiting;
        return ReadNode(roots.front(), positionOf, visiting);
    }
} // namespace GOAT::GraphEditor
