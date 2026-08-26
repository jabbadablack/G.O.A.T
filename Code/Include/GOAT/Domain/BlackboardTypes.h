#pragma once

#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/base.h>

namespace AZ
{
    class ReflectContext;
}

namespace GOAT
{
    //! Which lifetime a blackboard variable belongs to.
    enum class BlackboardScope : AZ::u8
    {
        Global,  //!< One shared instance for the whole world.
        Agent,   //!< One instance per agent.
        Squad,   //!< One instance per named squad.
        Count
    };

    //! Value kinds a blackboard variable may hold.
    enum class BlackboardType : AZ::u8
    {
        Bool,
        Int,
        Float,
        Vector3,
        EntityId,
        Name,
        Quaternion,
        Transform,
        EntityIdList,
        Count
    };

    //! Reflects the blackboard enums for serialization and scripting.
    void ReflectBlackboardTypes(AZ::ReflectContext* context);

    //! Returns the C++ type a blackboard type stores, or a null id when unsupported.
    AZ::TypeId ToTypeId(BlackboardType type);

    //! Returns the blackboard type for a C++ type, or Count when unsupported.
    BlackboardType FromTypeId(const AZ::TypeId& typeId);

    //! Returns a readable name for errors and console output.
    const char* ToString(BlackboardType type);

    //! Returns a readable name for errors and console output.
    const char* ToString(BlackboardScope scope);
} // namespace GOAT

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(GOAT::BlackboardScope, "{B4DF3B43-02B1-494C-97C4-F7CF250E2637}");
    AZ_TYPE_INFO_SPECIALIZE(GOAT::BlackboardType, "{5C87FAF9-C387-4554-AF17-E0F99AD62EBD}");
} // namespace AZ
