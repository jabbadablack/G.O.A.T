#pragma once

#include <GraphCanvas/Widgets/GraphCanvasMimeEvent.h>
#include <GraphCanvas/Widgets/NodePalette/TreeItems/DraggableNodePaletteTreeItem.h>

#include <AzCore/std/string/string.h>

namespace GOAT::GraphEditor
{
    //! Creates a node for one word. GraphModel's own palette item templates on the C++ node
    //! class, and every word here is the same class with a different name, so the name has to
    //! travel on the event instead.
    class CreateProgramNodeMimeEvent final
        : public GraphCanvas::GraphCanvasMimeEvent
    {
    public:
        AZ_RTTI(CreateProgramNodeMimeEvent, "{2D8E7E3B-9E44-4C9C-9E5B-40B7F9B2A8D1}",
            GraphCanvas::GraphCanvasMimeEvent);
        AZ_CLASS_ALLOCATOR(CreateProgramNodeMimeEvent, AZ::SystemAllocator);

        static void Reflect(AZ::ReflectContext* context);

        CreateProgramNodeMimeEvent() = default;
        explicit CreateProgramNodeMimeEvent(AZStd::string typeName);

        bool ExecuteEvent(const AZ::Vector2& mouseDropPosition, AZ::Vector2& dropPosition,
            const AZ::EntityId& graphCanvasSceneId) override;

    private:
        AZStd::string m_typeName;
    };

    //! One palette row, dragged onto the canvas to make a node of that word.
    class ProgramNodePaletteItem final
        : public GraphCanvas::DraggableNodePaletteTreeItem
    {
    public:
        AZ_CLASS_ALLOCATOR(ProgramNodePaletteItem, AZ::SystemAllocator);

        ProgramNodePaletteItem(AZStd::string typeName, AZStd::string_view titlePalette,
            GraphCanvas::EditorId editorId);

        GraphCanvas::GraphCanvasMimeEvent* CreateMimeEvent() const override;

    private:
        AZStd::string m_typeName;
    };
} // namespace GOAT::GraphEditor
