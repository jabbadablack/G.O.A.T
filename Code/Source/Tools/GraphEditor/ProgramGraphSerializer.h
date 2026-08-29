#pragma once

#include <GOAT/Assets/ProgramAsset.h>
#include <Tools/GraphEditor/ProgramNode.h>

#include <AzCore/Math/Vector2.h>
#include <AzCore/Outcome/Outcome.h>
#include <AzCore/std/functional.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT::GraphEditor
{
    //! One node to place, and what it hangs from. Produced ahead of the canvas so the layout
    //! can be checked without a running editor.
    struct PlacedNode final
    {
        AZStd::shared_ptr<ProgramNode> m_node;
        AZ::Vector2 m_position = AZ::Vector2::CreateZero();
        //! Index into the placement list, or -1 for the root.
        int m_parent = -1;
        //! True when the parent holds this in its service list rather than its children.
        bool m_isService = false;
    };

    //! Where a node sits on the canvas. Supplied by the window, so the round trip itself
    //! never depends on a canvas existing.
    using PositionLookup = AZStd::function<AZ::Vector2(GraphModel::ConstNodePtr)>;

    //! True when anything in the program says where it should sit. A program Lua declared, or
    //! one just created, says nothing, and the window lays it out from measured node sizes.
    bool HasAuthoredLayout(const AuthoredNode& root);

    //! Turns an authored program into the nodes a canvas should hold.
    //! Nodes with no authored position are laid out left to right, in execution order downwards.
    AZStd::vector<PlacedNode> FromAuthored(const AuthoredNode& root, GraphModel::GraphPtr graph);

    //! Reads a canvas back into an authored program.
    //! Siblings are ordered by how far down the canvas they sit, because that is what the
    //! author sees and what execution order means.
    AZ::Outcome<AuthoredNode, AZStd::string> ToAuthored(
        GraphModel::GraphPtr graph, const PositionLookup& positionOf);
} // namespace GOAT::GraphEditor
