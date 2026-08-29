#pragma once

#include <GOAT/Domain/BlackboardTypes.h>

#include <GraphModel/Model/GraphContext.h>

namespace GOAT::GraphEditor
{
    //! Data types a slot may carry. One per blackboard type, plus the execution edge
    //! that connects a parent to a child.
    enum class ProgramDataType : GraphModel::DataType::Enum
    {
        Execution = 0,
        Bool,
        Int,
        Float,
        Vector3,
        EntityId,
        Name,
        Quaternion,
        Transform,
        EntityIdList,
    };

    //! Maps a blackboard type onto the data type a slot of that type carries.
    ProgramDataType ToDataType(BlackboardType type);

    class GraphContext final
        : public GraphModel::GraphContext
    {
    public:
        AZ_CLASS_ALLOCATOR(GraphContext, AZ::SystemAllocator);

        static void SetInstance(AZStd::shared_ptr<GraphContext> context);
        static AZStd::shared_ptr<GraphContext> GetInstance();

        GraphContext();
        ~GraphContext() override = default;

    private:
        static AZStd::shared_ptr<GraphContext> s_instance;
    };
} // namespace GOAT::GraphEditor
