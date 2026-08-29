#pragma once

#include <GraphCanvas/Editor/EditorTypes.h>

namespace GOAT::GraphEditor
{
    inline const GraphCanvas::EditorId ProgramEditorId = AZ_CRC_CE("GOATProgramEditor");

    inline constexpr const char* SystemName = "GOAT Program Editor";
    inline constexpr const char* ModuleFileExtension = ".goat";
    inline constexpr const char* MimeEventType = "goat/node-palette-mime-event";
    inline constexpr const char* SaveIdentifier = "GOATProgramEditor";
    inline constexpr const char* StyleSheet = "GOAT/StyleSheet/graphcanvas_style.json";

    //! The slot every node is entered through, and the two a node may hold children in.
    //! Services are a list of their own on an authored node, so they get a slot of their own.
    inline constexpr const char* ParentSlotId = "Parent";
    inline constexpr const char* ChildrenSlotId = "Children";
    inline constexpr const char* ServicesSlotId = "Services";

    //! Prefix distinguishing a property slot from the two structural ones.
    inline constexpr const char* PropertySlotPrefix = "p_";
} // namespace GOAT::GraphEditor
