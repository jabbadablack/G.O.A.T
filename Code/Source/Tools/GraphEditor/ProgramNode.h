#pragma once

#include <GOAT/Domain/NodeType.h>

#include <GraphModel/Model/Node.h>

namespace GOAT::GraphEditor
{
    //! One node on the canvas, for any word in the node type registry.
    //! There is a single class rather than one per word because the registry already describes
    //! everything a node needs: its arity, its category and its typed parameter list. A backend
    //! that registers a word gets a node in the editor without writing one.
    class ProgramNode final
        : public GraphModel::Node
    {
    public:
        AZ_CLASS_ALLOCATOR(ProgramNode, AZ::SystemAllocator);
        AZ_RTTI(ProgramNode, "{6A9A8F9E-4C2E-4E8B-9C6B-2E5B6E1A4C77}", GraphModel::Node);

        static void Reflect(AZ::ReflectContext* context);

        //! The title palette a word of this kind and paradigm is drawn with.
        static const char* TitlePalette(const NodeTypeDescriptor& descriptor);

        ProgramNode() = default;
        ProgramNode(GraphModel::GraphPtr graph, const AZStd::string& typeName);

        //! The word this node was authored as.
        const AZStd::string& GetTypeName() const;

        //! The registry entry for this node's word, or nullptr when nothing registers it.
        const NodeTypeDescriptor* GetDescriptor() const;

        //! Slot id a parameter of this name is held in.
        static AZStd::string PropertySlotId(AZStd::string_view parameterName);

        // GraphModel::Node
        const char* GetTitle() const override;
        const char* GetSubTitle() const override;
        void PostLoadSetup(GraphModel::GraphPtr graph, GraphModel::NodeId id) override;

    protected:
        void RegisterSlots() override;

    private:
        //! Caches what the registry says, so the title pointers stay valid.
        void Resolve();

        AZStd::string m_typeName;
        AZStd::string m_title;
        AZStd::string m_subTitle;
    };
} // namespace GOAT::GraphEditor
