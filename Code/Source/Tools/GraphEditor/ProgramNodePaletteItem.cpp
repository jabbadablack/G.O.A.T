#include <Tools/GraphEditor/ProgramNodePaletteItem.h>
#include <Tools/GraphEditor/ProgramNode.h>

#include <GraphModel/GraphModelBus.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/smart_ptr/make_shared.h>

namespace GOAT::GraphEditor
{
    void CreateProgramNodeMimeEvent::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CreateProgramNodeMimeEvent, GraphCanvas::GraphCanvasMimeEvent>()
                ->Version(0)
                ->Field("typeName", &CreateProgramNodeMimeEvent::m_typeName);
        }
    }

    CreateProgramNodeMimeEvent::CreateProgramNodeMimeEvent(AZStd::string typeName)
        : m_typeName(AZStd::move(typeName))
    {
    }

    bool CreateProgramNodeMimeEvent::ExecuteEvent(
        [[maybe_unused]] const AZ::Vector2& mouseDropPosition, AZ::Vector2& dropPosition,
        const AZ::EntityId& graphCanvasSceneId)
    {
        GraphModel::GraphPtr graph;
        GraphModelIntegration::GraphManagerRequestBus::BroadcastResult(
            graph, &GraphModelIntegration::GraphManagerRequests::GetGraph, graphCanvasSceneId);
        if (graph == nullptr)
        {
            return false;
        }

        AZStd::shared_ptr<GraphModel::Node> node = AZStd::make_shared<ProgramNode>(graph, m_typeName);
        GraphModelIntegration::GraphControllerRequestBus::EventResult(
            m_createdNodeId, graphCanvasSceneId, &GraphModelIntegration::GraphControllerRequests::AddNode,
            node, dropPosition);
        return true;
    }

    ProgramNodePaletteItem::ProgramNodePaletteItem(
        AZStd::string typeName, AZStd::string_view titlePalette, GraphCanvas::EditorId editorId)
        : DraggableNodePaletteTreeItem(typeName.c_str(), editorId)
        , m_typeName(AZStd::move(typeName))
    {
        if (!titlePalette.empty())
        {
            SetTitlePalette(AZStd::string(titlePalette));
        }
    }

    GraphCanvas::GraphCanvasMimeEvent* ProgramNodePaletteItem::CreateMimeEvent() const
    {
        return aznew CreateProgramNodeMimeEvent(m_typeName);
    }
} // namespace GOAT::GraphEditor
