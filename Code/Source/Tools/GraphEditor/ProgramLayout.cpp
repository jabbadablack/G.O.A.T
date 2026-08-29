#include <Tools/GraphEditor/ProgramLayout.h>

#include <AzCore/std/algorithm.h>

namespace GOAT::GraphEditor
{
    namespace
    {
        //! Space left between columns and between sibling nodes.
        constexpr float ColumnGap = 90.0f;
        constexpr float RowGap = 40.0f;
    } // namespace

    AZStd::vector<AZ::Vector2> LayoutProgram(const AZStd::vector<LayoutNode>& nodes)
    {
        const size_t count = nodes.size();
        AZStd::vector<AZ::Vector2> positions(count, AZ::Vector2::CreateZero());
        if (count == 0)
        {
            return positions;
        }

        AZStd::vector<int> depth(count, 0);
        AZStd::vector<bool> hasChild(count, false);
        for (size_t i = 0; i < count; ++i)
        {
            if (nodes[i].m_parent >= 0)
            {
                const size_t parent = static_cast<size_t>(nodes[i].m_parent);
                depth[i] = depth[parent] + 1;
                hasChild[parent] = true;
            }
        }

        // A column is as wide as its widest node, so a node with long property values does not
        // run into the column beside it.
        AZStd::vector<float> columnX;
        for (size_t i = 0; i < count; ++i)
        {
            const size_t d = static_cast<size_t>(depth[i]);
            if (columnX.size() <= d)
            {
                columnX.resize(d + 1, 0.0f);
            }
            columnX[d] = AZStd::max(columnX[d], nodes[i].m_width);
        }
        float running = 0.0f;
        for (float& x : columnX)
        {
            const float widest = x;
            x = running;
            running += widest + ColumnGap;
        }

        AZStd::vector<float> top(count, 0.0f);
        AZStd::vector<float> bottom(count, 0.0f);

        // Leaves take their bands in the order they were authored, so the child that runs first
        // is the one that sits highest.
        float cursor = 0.0f;
        for (size_t i = 0; i < count; ++i)
        {
            if (!hasChild[i])
            {
                top[i] = cursor;
                bottom[i] = cursor + nodes[i].m_height;
                cursor = bottom[i] + RowGap;
            }
        }

        // Then parents centre on what they own. A child always comes after its parent, so walking
        // backwards reaches a parent only once everything under it is placed.
        for (size_t step = count; step > 0; --step)
        {
            const size_t i = step - 1;
            if (!hasChild[i])
            {
                continue;
            }

            bool any = false;
            float childTop = 0.0f;
            float childBottom = 0.0f;
            for (size_t c = 0; c < count; ++c)
            {
                if (nodes[c].m_parent != static_cast<int>(i))
                {
                    continue;
                }
                childTop = any ? AZStd::min(childTop, top[c]) : top[c];
                childBottom = any ? AZStd::max(childBottom, bottom[c]) : bottom[c];
                any = true;
            }

            const float centre = (childTop + childBottom) * 0.5f;
            top[i] = centre - nodes[i].m_height * 0.5f;
            bottom[i] = centre + nodes[i].m_height * 0.5f;
        }

        for (size_t i = 0; i < count; ++i)
        {
            positions[i] = AZ::Vector2(columnX[static_cast<size_t>(depth[i])], top[i]);
        }
        return positions;
    }
} // namespace GOAT::GraphEditor
