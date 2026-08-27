#include <Animation/AnimateAction.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Name/NameDictionary.h>

#include <Integration/AnimGraphComponentBus.h>
#include <Integration/SimpleMotionComponentBus.h>

namespace GOAT_Animation
{
    namespace
    {
        //! Seconds this agent has been playing, kept in the agent's scratch.
        float& Elapsed(const GOAT::ActionContext& context)
        {
            return *reinterpret_cast<float*>(context.m_scratch->data());
        }

        static_assert(sizeof(float) <= AZStd::tuple_size<GOAT::ActionScratch>::value,
            "Animation state does not fit in the action scratch");

        //! Resolves a motion named by path to its asset id, or an invalid id when it is unknown.
        AZ::Data::AssetId FindMotion(const AZ::Name& path)
        {
            AZ_Assert(!path.IsEmpty(), "A motion is only looked up when the node named one");

            AZ::Data::AssetId assetId;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                assetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath, path.GetCStr(), AZ::Data::s_invalidAssetType,
                false);

            AZ_Warning("GOAT", assetId.IsValid(), "play_motion names motion '%s', which is not in the asset catalog",
                path.GetCStr());
            return assetId;
        }
    } // namespace

    AZ::Name AnimateAction::GetName() const
    {
        return AZ_NAME_LITERAL("animate");
    }

    bool AnimateAction::WriteFromBlackboard(const GOAT::ActionContext& context, const char* parameter) const
    {
        AZ_Assert(parameter != nullptr, "An anim graph parameter is always written by name");

        const GOAT::BlackboardKey key = context.m_request->m_targetKey;
        AZ_Assert(key.IsValid(), "This is only reached when the node named a blackboard variable");

        using Requests = EMotionFX::Integration::AnimGraphComponentRequestBus;
        const AZ::EntityId entity = context.m_entity;

        // Which setter runs follows the variable's declared type, so a tree cannot silently
        // push a bool into a float parameter and get a zero.
        switch (key.GetType())
        {
        case GOAT::BlackboardType::Bool:
            if (const bool* value = context.m_blackboard->Find<bool>(key, context.m_agent))
            {
                Requests::Event(entity, &Requests::Events::SetNamedParameterBool, parameter, *value);
                return true;
            }
            return false;

        case GOAT::BlackboardType::Float:
            if (const float* value = context.m_blackboard->Find<float>(key, context.m_agent))
            {
                Requests::Event(entity, &Requests::Events::SetNamedParameterFloat, parameter, *value);
                return true;
            }
            return false;

        case GOAT::BlackboardType::Int:
            if (const AZ::s64* value = context.m_blackboard->Find<AZ::s64>(key, context.m_agent))
            {
                Requests::Event(entity, &Requests::Events::SetNamedParameterFloat, parameter,
                    static_cast<float>(*value));
                return true;
            }
            return false;

        case GOAT::BlackboardType::Vector3:
            if (const AZ::Vector3* value = context.m_blackboard->Find<AZ::Vector3>(key, context.m_agent))
            {
                Requests::Event(entity, &Requests::Events::SetNamedParameterVector3, parameter, *value);
                return true;
            }
            return false;

        default:
            AZ_Error("GOAT", false, "animate cannot push a %s variable into anim graph parameter '%s'",
                GOAT::ToString(key.GetType()), parameter);
            return false;
        }
    }

    void AnimateAction::Begin(const GOAT::ActionContext& context)
    {
        AZ_Assert(context.m_request != nullptr, "An action always begins with a request");
        AZ_Assert(context.m_blackboard != nullptr, "An action always begins with a blackboard");

        const AZ::Name parameter = context.m_request->m_tag;
        AZ_Assert(!parameter.IsEmpty(), "An animate leaf always names the parameter it writes");
        if (parameter.IsEmpty())
        {
            AZ_Error("GOAT", false, "animate ran with no parameter name, so nothing was written");
            return;
        }

        if (context.m_request->m_targetKey.IsValid())
        {
            WriteFromBlackboard(context, parameter.GetCStr());
            return;
        }

        // No variable named, so the node's own number is the value.
        using Requests = EMotionFX::Integration::AnimGraphComponentRequestBus;
        Requests::Event(context.m_entity, &Requests::Events::SetNamedParameterFloat, parameter.GetCStr(),
            context.m_request->m_amount);
    }

    GOAT::ActionResult AnimateAction::Step(
        [[maybe_unused]] const GOAT::ActionContext& context, [[maybe_unused]] float deltaTime)
    {
        // Setting a parameter is a state change, not something that takes time.
        return GOAT::ActionResult::Success;
    }


    AZ::Name PlayMotionAction::GetName() const
    {
        return AZ_NAME_LITERAL("play_motion");
    }

    void PlayMotionAction::Begin(const GOAT::ActionContext& context)
    {
        AZ_Assert(context.m_scratch != nullptr, "A motion always runs with agent scratch to time it");
        AZ_Assert(context.m_request != nullptr, "An action always begins with a request");

        Elapsed(context) = 0.0f;

        using Requests = EMotionFX::Integration::SimpleMotionComponentRequestBus;

        // A named motion switches the component over; an unnamed one plays what it already holds.
        if (!context.m_request->m_tag.IsEmpty())
        {
            const AZ::Data::AssetId motion = FindMotion(context.m_request->m_tag);
            if (motion.IsValid())
            {
                Requests::Event(context.m_entity, &Requests::Events::Motion, motion);
            }
        }

        Requests::Event(context.m_entity, &Requests::Events::PlayMotion);
    }

    GOAT::ActionResult PlayMotionAction::Step(const GOAT::ActionContext& context, float deltaTime)
    {
        AZ_Assert(deltaTime >= 0.0f, "A motion cannot be stepped backwards in time");

        float& elapsed = Elapsed(context);
        elapsed += deltaTime;

        // The node's own number wins when it names one, so a tree can cut a long clip short.
        float duration = context.m_request->m_amount;
        if (duration <= 0.0f)
        {
            using Requests = EMotionFX::Integration::SimpleMotionComponentRequestBus;
            Requests::EventResult(duration, context.m_entity, &Requests::Events::GetDuration);
        }

        AZ_Warning("GOAT", duration > 0.0f,
            "play_motion found no duration on entity %s, so it succeeds immediately",
            context.m_entity.ToString().c_str());

        return elapsed >= duration ? GOAT::ActionResult::Success : GOAT::ActionResult::Running;
    }

} // namespace GOAT_Animation
