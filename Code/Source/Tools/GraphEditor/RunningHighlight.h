#pragma once

#include <GOAT/Domain/AgentDebug.h>

#include <GraphCanvas/Editor/EditorTypes.h>
#include <GraphModel/Model/Node.h>

#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/functional.h>

namespace GOAT::GraphEditor
{
    //! Which canvas node a step of an authored path leads to, or null.
    using NodeLookup = AZStd::function<GraphModel::NodePtr(const AZStd::vector<size_t>&)>;

    //! Draws the path an agent is running on the canvas.
    //!
    //! Drawn as a graphics effect rather than by overriding a node's palette: an effect is not
    //! part of the graph, so it neither reports the program as modified -- which would ask for
    //! validation, which paints, which asks again -- nor fights the red a failing node is
    //! already painted. A node can be wrong and running at the same time and look like both.
    class RunningHighlight final
    {
    public:
        ~RunningHighlight();

        //! Lights the given path and puts out whatever is no longer on it. Paths naming a
        //! program other than the one on screen are skipped, because a node inlined from
        //! another tree is not on this canvas to light.
        void Show(const GraphCanvas::GraphId& graphId, const AZ::Name& programOnScreen,
            const AZStd::vector<ProgramNodeRef>& path, const NodeLookup& nodeAt);

        //! Puts everything out, for a program being closed or an agent going away.
        void Clear();

        //! How many nodes are lit, which is what a test can check without a canvas.
        size_t GetLitCount() const { return m_lit.size(); }

    private:
        //! The effect lighting one node, keyed by the node so a path that has not changed is
        //! left alone rather than put out and lit again every poll.
        AZStd::unordered_map<GraphCanvas::NodeId, GraphCanvas::GraphicsEffectId> m_lit;
        GraphCanvas::GraphId m_graphId;
    };
} // namespace GOAT::GraphEditor
