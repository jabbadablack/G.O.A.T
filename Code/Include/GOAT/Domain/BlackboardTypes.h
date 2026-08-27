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



    // These two are defined here rather than in a .cpp because the gem's API target is headers
    // only: a module gem in this tree links no GOAT object, so anything it calls must be inline.

    //! Returns a readable name for errors and console output.
    inline const char* ToString(BlackboardType type)
    {
        switch (type)
        {
        case BlackboardType::Bool:
            return "Bool";
        case BlackboardType::Int:
            return "Int";
        case BlackboardType::Float:
            return "Float";
        case BlackboardType::Vector3:
            return "Vector3";
        case BlackboardType::EntityId:
            return "EntityId";
        case BlackboardType::Name:
            return "Name";
        case BlackboardType::Quaternion:
            return "Quaternion";
        case BlackboardType::Transform:
            return "Transform";
        case BlackboardType::EntityIdList:
            return "EntityIdList";
        default:
            return "<invalid>";
        }
    }

    //! Returns a readable name for errors and console output.
    inline const char* ToString(BlackboardScope scope)
    {
        switch (scope)
        {
        case BlackboardScope::Global:
            return "Global";
        case BlackboardScope::Agent:
            return "Agent";
        case BlackboardScope::Squad:
            return "Squad";
        default:
            return "<invalid>";
        }
    }
} // namespace GOAT

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(GOAT::BlackboardScope, "{B4DF3B43-02B1-494C-97C4-F7CF250E2637}");
    AZ_TYPE_INFO_SPECIALIZE(GOAT::BlackboardType, "{5C87FAF9-C387-4554-AF17-E0F99AD62EBD}");
} // namespace AZ
