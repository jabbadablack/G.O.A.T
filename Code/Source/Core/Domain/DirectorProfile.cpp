#include <GOAT/Domain/DirectorProfile.h>

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

namespace GOAT
{
    void DirectorReach::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Class<DirectorReach>()
            ->Version(1)
            ->Field("Squad", &DirectorReach::m_squad)
            ->Field("Tree", &DirectorReach::m_tree)
            ->Field("Radius", &DirectorReach::m_radius)
            ->Field("Filter", &DirectorReach::m_filter);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (editContext == nullptr)
        {
            return;
        }

        editContext->Class<DirectorReach>("Reach", "Which agents this director governs")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &DirectorReach::m_squad, "Squad",
                "Governs only this squad. Leave empty for any.")
            ->DataElement(AZ::Edit::UIHandlers::Default, &DirectorReach::m_tree, "Tree",
                "Governs only agents currently running this tree. Leave empty for any.")
            ->DataElement(AZ::Edit::UIHandlers::Default, &DirectorReach::m_radius, "Radius",
                "Governs only agents this close. Zero for any distance.")
                ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
            ->DataElement(AZ::Edit::UIHandlers::Default, &DirectorReach::m_filter, "Filter",
                "A reach filter a module contributed, such as path_distance from the navigation "
                "gem. Leave empty for plain straight line distance.");
    }

    void DirectorProfile::Reflect(AZ::ReflectContext* context)
    {
        auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext == nullptr)
        {
            return;
        }

        serializeContext->Class<DirectorProfile>()
            ->Version(1)
            ->Field("Reach", &DirectorProfile::m_reach)
            ->Field("Priority", &DirectorProfile::m_priority)
            ->Field("Cooldown", &DirectorProfile::m_cooldownSeconds);

        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (editContext == nullptr)
        {
            return;
        }

        editContext->Class<DirectorProfile>("Governs", "Which agents, and how forcefully")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
            ->DataElement(AZ::Edit::UIHandlers::Default, &DirectorProfile::m_reach, "Reach", "")
            ->DataElement(AZ::Edit::UIHandlers::SpinBox, &DirectorProfile::m_priority, "Priority",
                "Higher outranks lower when two directors command the same agent. Zero is what an "
                "agent switching its own tree carries, so leave this above zero.")
                ->Attribute(AZ::Edit::Attributes::Min, 0)
                ->Attribute(AZ::Edit::Attributes::Max, 255)
            ->DataElement(AZ::Edit::UIHandlers::Default, &DirectorProfile::m_cooldownSeconds, "Cooldown",
                "How long before this director may command the same agent the same way again. "
                "Switching a tree stops whatever the agent was doing, so ordering one every tick "
                "would leave it permanently restarting.")
                ->Attribute(AZ::Edit::Attributes::Min, 0.0f);
    }
} // namespace GOAT
