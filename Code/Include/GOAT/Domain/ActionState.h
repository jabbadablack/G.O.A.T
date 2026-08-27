#pragma once

#include <GOAT/Domain/BlackboardKey.h>
#include <GOAT/GOATTypeIds.h>

#include <AzCore/Math/Vector3.h>
#include <AzCore/Name/Name.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/Component/EntityId.h>

namespace GOAT
{
    //! Index of a registered action verb.
    //! Not a closed enum: modules and backends register additional verbs at runtime.
    using ActionStateId = AZ::u8;

    //! Verbs the core always provides. Registered verbs start at FirstRegistered.
    namespace CoreActions
    {
        inline constexpr ActionStateId Invalid = 0;
        inline constexpr ActionStateId Wait = 1;
        inline constexpr ActionStateId RunScript = 2;
        inline constexpr ActionStateId FirstRegistered = 3;
    } // namespace CoreActions

    //! Outcome of advancing an action state.
    enum class ActionResult : AZ::u8
    {
        Running,  //!< Still in progress; the FSM stays in this state.
        Success,
        Failure
    };

    //! Parameters for one action, produced by a backend and consumed by an action state.
    //! Fields an action does not use are simply left at their defaults.
    struct ActionRequest final
    {
        AZ_TYPE_INFO(ActionRequest, ActionRequestTypeId);

        static void Reflect(AZ::ReflectContext* context);

        //! Which registered verb to run.
        ActionStateId m_action = CoreActions::Invalid;
        //! Blackboard slot to read the target from; overrides m_position when valid.
        BlackboardKey m_targetKey;
        //! Literal target position, used when m_targetKey is not set.
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        //! Target entity, for actions that act on another entity.
        AZ::EntityId m_targetEntity;
        //! Names the thing to run: a script node, an animation clip, a bark line.
        AZ::Name m_tag;
        //! The one scalar a verb needs: seconds for `wait`, speed for a movement verb.
        //! Authoring fills it from any numeric node property other than `tolerance`.
        float m_amount = 0.0f;
        //! How close counts as arrived, for movement-like verbs.
        float m_tolerance = 0.0f;
    };

    //! Reflects the action enums for serialization and scripting.
    void ReflectActionTypes(AZ::ReflectContext* context);
} // namespace GOAT

namespace AZ
{
    AZ_TYPE_INFO_SPECIALIZE(GOAT::ActionResult, "{8C2798C9-7804-420A-B545-FE721A43B849}");
} // namespace AZ
