#include <Tools/GraphEditor/RunningHighlight.h>

#include <GraphCanvas/Components/SceneBus.h>
#include <GraphModel/GraphModelBus.h>

#include <QColor>
#include <QPen>

namespace GOAT::GraphEditor
{
    namespace
    {
        //! The green a running node is drawn in, which is the one the stylesheet already names
        //! for it so the canvas and the outline cannot drift apart.
        constexpr int RunningRed = 0x3D;
        constexpr int RunningGreen = 0xA4;
        constexpr int RunningBlue = 0x3D;

        //! Slow enough to read as alive rather than as an alarm.
        constexpr int PulseMilliseconds = 1000;
        constexpr qreal OutlineWidth = 4.0;
    } // namespace

    RunningHighlight::~RunningHighlight()
    {
        Clear();
    }

    void RunningHighlight::Show(const GraphCanvas::GraphId& graphId, const AZ::Name& programOnScreen,
        const AZStd::vector<ProgramNodeRef>& path, const NodeLookup& nodeAt)
    {
        if (m_graphId != graphId)
        {
            // A different program is on screen now, so nothing lit for the old one applies.
            Clear();
            m_graphId = graphId;
        }

        AZStd::unordered_map<GraphCanvas::NodeId, GraphCanvas::GraphicsEffectId> stillLit;
        for (const ProgramNodeRef& step : path)
        {
            if (step.m_program != programOnScreen)
            {
                // Authored in a subtree this canvas is not showing. Saying nothing is better
                // than lighting whichever node happens to sit at that index here.
                continue;
            }

            AZStd::vector<size_t> indices;
            indices.reserve(step.m_path.size());
            for (const AZ::u16 index : step.m_path)
            {
                indices.push_back(index);
            }

            GraphModel::NodePtr node = nodeAt(indices);
            if (node == nullptr)
            {
                continue;
            }

            GraphCanvas::NodeId nodeId;
            GraphModelIntegration::GraphControllerRequestBus::EventResult(
                nodeId, graphId, &GraphModelIntegration::GraphControllerRequests::GetNodeIdByNode, node);
            if (!nodeId.IsValid())
            {
                continue;
            }

            if (const auto already = m_lit.find(nodeId); already != m_lit.end())
            {
                stillLit[nodeId] = already->second;
                m_lit.erase(already);
                continue;
            }

            GraphCanvas::SceneMemberGlowOutlineConfiguration glow;
            glow.m_sceneMember = nodeId;
            glow.m_pen = QPen(QColor(RunningRed, RunningGreen, RunningBlue));
            glow.m_pen.setWidthF(OutlineWidth);
            glow.m_pulseRate = AZStd::chrono::milliseconds(PulseMilliseconds);
            glow.m_zValue = 1;

            GraphCanvas::GraphicsEffectId effect;
            GraphCanvas::SceneRequestBus::EventResult(
                effect, graphId, &GraphCanvas::SceneRequests::CreateGlowOnSceneMember, glow);
            if (effect.IsValid())
            {
                stillLit[nodeId] = effect;
            }
        }

        // Whatever is left in m_lit was on the path last time and is not now.
        for (const auto& [nodeId, effect] : m_lit)
        {
            GraphCanvas::SceneRequestBus::Event(
                m_graphId, &GraphCanvas::SceneRequests::CancelGraphicsEffect, effect);
        }
        m_lit = AZStd::move(stillLit);
    }

    void RunningHighlight::Clear()
    {
        for (const auto& [nodeId, effect] : m_lit)
        {
            GraphCanvas::SceneRequestBus::Event(
                m_graphId, &GraphCanvas::SceneRequests::CancelGraphicsEffect, effect);
        }
        m_lit.clear();
    }
} // namespace GOAT::GraphEditor
