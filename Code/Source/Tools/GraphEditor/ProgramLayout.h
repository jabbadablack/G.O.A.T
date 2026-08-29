#pragma once

#include <AzCore/Math/Vector2.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT::GraphEditor
{
    //! One node to place, as measured on the canvas.
    struct LayoutNode final
    {
        //! Index of the node this one runs under, or -1 for the root.
        //! A node always appears after its parent, which is what lets the pass run in one sweep.
        int m_parent = -1;
        float m_width = 0.0f;
        float m_height = 0.0f;
    };

    //! Places a program left to right, a column per depth, siblings top to bottom in the order
    //! they run. Sizes come from the canvas rather than being assumed, because a node carries its
    //! properties on its face and so has no height worth guessing.
    AZStd::vector<AZ::Vector2> LayoutProgram(const AZStd::vector<LayoutNode>& nodes);
} // namespace GOAT::GraphEditor
