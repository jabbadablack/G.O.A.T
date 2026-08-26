#include <GOAT/Domain/ActionState.h>

#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void ActionRequest::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<ActionRequest>()
                ->Version(1)
                ->Field("Action", &ActionRequest::m_action)
                ->Field("TargetKey", &ActionRequest::m_targetKey)
                ->Field("Position", &ActionRequest::m_position)
                ->Field("TargetEntity", &ActionRequest::m_targetEntity)
                ->Field("Tag", &ActionRequest::m_tag)
                ->Field("Duration", &ActionRequest::m_duration)
                ->Field("Tolerance", &ActionRequest::m_tolerance);
        }
    }

    void ReflectActionTypes(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Enum<ActionResult>()
                ->Value("Running", ActionResult::Running)
                ->Value("Success", ActionResult::Success)
                ->Value("Failure", ActionResult::Failure);
        }

        ActionRequest::Reflect(context);
    }
} // namespace GOAT
