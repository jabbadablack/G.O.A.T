#include <Navigation/NavigationTarget.h>

#include <GOAT/Interfaces/IBlackboardSystem.h>

#include <AzCore/Component/TransformBus.h>

namespace GOAT_Navigation
{
    bool ReadActionTarget(const GOAT::ActionContext& context, AZ::Vector3& outTarget)
    {
        AZ_Assert(context.m_request != nullptr, "A spatial verb always runs with a request");
        if (context.m_request == nullptr)
        {
            return false;
        }

        const GOAT::ActionRequest& request = *context.m_request;
        if (!request.m_targetKey.IsValid())
        {
            outTarget = request.m_position;
            return true;
        }

        AZ_Assert(context.m_blackboard != nullptr, "Reading a blackboard target needs a blackboard");
        if (context.m_blackboard == nullptr)
        {
            return false;
        }

        const AZ::Vector3* value = context.m_blackboard->Find<AZ::Vector3>(request.m_targetKey, context.m_agent);
        AZ_Warning("GOAT", value != nullptr,
            "A spatial verb reads a blackboard target that agent %u has no storage for", context.m_agent.GetIndex());
        if (value == nullptr)
        {
            return false;
        }

        outTarget = *value;
        return true;
    }

    AZ::Vector3 ReadActionPosition(const GOAT::ActionContext& context)
    {
        AZ_Assert(context.m_entity.IsValid(), "An agent always drives a valid entity");

        AZ::Vector3 position = AZ::Vector3::CreateZero();
        AZ::TransformBus::EventResult(position, context.m_entity, &AZ::TransformInterface::GetWorldTranslation);
        return position;
    }
} // namespace GOAT_Navigation
