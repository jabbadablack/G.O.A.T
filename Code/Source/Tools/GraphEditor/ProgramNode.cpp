#include <Tools/GraphEditor/ProgramNode.h>
#include <Tools/GraphEditor/Core.h>
#include <Tools/GraphEditor/GraphContext.h>

#include <GOAT/Interfaces/IAgentSystem.h>

#include <AzCore/Name/NameDictionary.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/smart_ptr/make_shared.h>

namespace GOAT::GraphEditor
{
    namespace
    {
        //! How many children a word of this kind may hold, as a slot count.
        //! A composite is open ended, a decorator holds one, a leaf holds none.
        constexpr int MaxCompositeChildren = 32;

        const NodeTypeDescriptor* FindDescriptor(const AZStd::string& typeName)
        {
            IAgentSystem* agents = AgentSystemInterface::Get();
            return agents != nullptr ? agents->FindNodeType(AZ::Name(typeName)) : nullptr;
        }
    } // namespace

    void ProgramNode::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ProgramNode, GraphModel::Node>()
                ->Version(0)
                ->Field("typeName", &ProgramNode::m_typeName);
        }
    }

    const char* ProgramNode::TitlePalette(const NodeTypeDescriptor& descriptor)
    {
        if (descriptor.m_backend == AZ_NAME_LITERAL("htn"))
        {
            return "TaskNetworkNodeTitlePalette";
        }
        if (descriptor.m_backend == AZ_NAME_LITERAL("utility"))
        {
            return "UtilityNodeTitlePalette";
        }

        switch (descriptor.m_kind)
        {
        case NodeKind::Composite: return "CompositeNodeTitlePalette";
        case NodeKind::Decorator: return "DecoratorNodeTitlePalette";
        case NodeKind::Service:   return "ServiceNodeTitlePalette";
        default:                  return "LeafNodeTitlePalette";
        }
    }

    ProgramNode::ProgramNode(GraphModel::GraphPtr graph, const AZStd::string& typeName)
        : GraphModel::Node(graph)
        , m_typeName(typeName)
    {
        Resolve();
        RegisterSlots();
        CreateSlotData();
    }

    const AZStd::string& ProgramNode::GetTypeName() const
    {
        return m_typeName;
    }

    const NodeTypeDescriptor* ProgramNode::GetDescriptor() const
    {
        return FindDescriptor(m_typeName);
    }

    AZStd::string ProgramNode::PropertySlotId(AZStd::string_view parameterName)
    {
        AZStd::string id(PropertySlotPrefix);
        id += parameterName;
        return id;
    }

    const char* ProgramNode::GetTitle() const
    {
        return m_title.c_str();
    }

    const char* ProgramNode::GetSubTitle() const
    {
        return m_subTitle.c_str();
    }

    void ProgramNode::PostLoadSetup(GraphModel::GraphPtr graph, GraphModel::NodeId id)
    {
        Resolve();
        GraphModel::Node::PostLoadSetup(graph, id);
    }

    void ProgramNode::Resolve()
    {
        m_title = m_typeName;
        const NodeTypeDescriptor* descriptor = FindDescriptor(m_typeName);
        m_subTitle = descriptor != nullptr ? descriptor->m_category : AZStd::string("Unknown");
    }

    void ProgramNode::RegisterSlots()
    {
        const NodeTypeDescriptor* descriptor = FindDescriptor(m_typeName);
        if (descriptor == nullptr)
        {
            // The word left with its gem. The node still loads, so the graph is not silently
            // rewritten, and validation is what reports it.
            return;
        }

        auto context = AZStd::static_pointer_cast<GraphContext>(GetGraphContext());
        AZ_Assert(context != nullptr, "A program node is always built in a program graph context");

        GraphModel::DataTypePtr execution =
            context->GetDataType(static_cast<GraphModel::DataType::Enum>(ProgramDataType::Execution));

        // Every word is entered from above, including the root, which simply leaves it unconnected.
        RegisterSlot(AZStd::make_shared<GraphModel::SlotDefinition>(
            GraphModel::SlotDirection::Input, GraphModel::SlotType::Event, ParentSlotId, "Parent",
            "The node this one runs under", GraphModel::DataTypeList{ execution }));

        // Arity is the slot layout, so a leaf cannot be given a child at all.
        const int maxChildren = descriptor->m_kind == NodeKind::Composite ? MaxCompositeChildren
            : descriptor->m_kind == NodeKind::Decorator                   ? 1
                                                                          : 0;
        if (maxChildren > 0)
        {
            RegisterSlot(AZStd::make_shared<GraphModel::SlotDefinition>(
                GraphModel::SlotDirection::Output, GraphModel::SlotType::Event, ChildrenSlotId, "Children",
                "What runs under this node, in order from the top", GraphModel::DataTypeList{ execution },
                AZStd::any{}, 1, maxChildren));
        }

        // Services are a list of their own on an authored node, and only a composite carries them.
        if (descriptor->m_kind == NodeKind::Composite)
        {
            RegisterSlot(AZStd::make_shared<GraphModel::SlotDefinition>(
                GraphModel::SlotDirection::Output, GraphModel::SlotType::Event, ServicesSlotId, "Services",
                "Services ticked while this subtree is active", GraphModel::DataTypeList{ execution },
                AZStd::any{}, 1, MaxCompositeChildren));
        }

        // Properties sit on the node face rather than becoming wires: an authored property is a
        // value, and no compiler reads a value produced by another node.
        for (const NodeParameter& parameter : descriptor->m_parameters)
        {
            GraphModel::DataTypePtr type =
                context->GetDataType(static_cast<GraphModel::DataType::Enum>(ToDataType(parameter.m_type)));

            AZStd::string label = parameter.m_name.GetCStr();
            if (parameter.m_required)
            {
                label += " *";
            }

            RegisterSlot(AZStd::make_shared<GraphModel::SlotDefinition>(
                GraphModel::SlotDirection::Input, GraphModel::SlotType::Property,
                PropertySlotId(parameter.m_name.GetStringView()), label,
                parameter.m_isBlackboardKey ? "Names a blackboard variable" : "A literal value",
                GraphModel::DataTypeList{ type }, type->GetDefaultValue()));
        }
    }
} // namespace GOAT::GraphEditor
