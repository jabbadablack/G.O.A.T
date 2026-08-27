#pragma once

#include <GOAT/GOATTypeIds.h>

#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>

namespace GOAT
{
    //! Index of a node within a compiled DecisionProgram.
    using NodeIndex = AZ::u32;

    //! Value meaning "no node".
    inline constexpr NodeIndex InvalidNodeIndex = static_cast<NodeIndex>(-1);

    //! What the tree wants done next; the input to a backend.
    struct Intent final
    {
        AZ_TYPE_INFO(Intent, IntentTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Backend asked to satisfy this intent. Empty means the built-in direct backend.
        AZ::Name m_backend;
        //! What to achieve, interpreted by the backend. A goal name for a planner.
        AZ::Name m_goal;
        //! Tree node this intent came from, for debugging and for resuming the walk.
        NodeIndex m_node = InvalidNodeIndex;
    };
} // namespace GOAT
