#pragma once

#include <GOAT/Domain/BlackboardTypes.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/std/containers/vector.h>

namespace GOAT
{
    //! The list of entities a BlackboardType::EntityIdList slot holds.
    using EntityIdList = AZStd::vector<AZ::EntityId>;

    //! Maps a C++ type to the blackboard type tag that stores it.
    //! Only the types named here may be stored on a blackboard.
    template<typename T>
    struct BlackboardTypeOf;

    template<>
    struct BlackboardTypeOf<bool>
    {
        static constexpr BlackboardType Value = BlackboardType::Bool;
    };
    template<>
    struct BlackboardTypeOf<AZ::s64>
    {
        static constexpr BlackboardType Value = BlackboardType::Int;
    };
    template<>
    struct BlackboardTypeOf<float>
    {
        static constexpr BlackboardType Value = BlackboardType::Float;
    };
    template<>
    struct BlackboardTypeOf<AZ::Vector3>
    {
        static constexpr BlackboardType Value = BlackboardType::Vector3;
    };
    template<>
    struct BlackboardTypeOf<AZ::EntityId>
    {
        static constexpr BlackboardType Value = BlackboardType::EntityId;
    };
    template<>
    struct BlackboardTypeOf<AZ::Name>
    {
        static constexpr BlackboardType Value = BlackboardType::Name;
    };
    template<>
    struct BlackboardTypeOf<AZ::Quaternion>
    {
        static constexpr BlackboardType Value = BlackboardType::Quaternion;
    };
    template<>
    struct BlackboardTypeOf<AZ::Transform>
    {
        static constexpr BlackboardType Value = BlackboardType::Transform;
    };
    template<>
    struct BlackboardTypeOf<EntityIdList>
    {
        static constexpr BlackboardType Value = BlackboardType::EntityIdList;
    };
} // namespace GOAT
