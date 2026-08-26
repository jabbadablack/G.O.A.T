
#include "GOAT_NavigationModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <GOAT_Navigation/GOAT_NavigationTypeIds.h>

#include <Clients/GOAT_NavigationSystemComponent.h>

namespace GOAT_Navigation
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(GOAT_NavigationModuleInterface,
        "GOAT_NavigationModuleInterface", GOAT_NavigationModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(GOAT_NavigationModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(GOAT_NavigationModuleInterface, AZ::SystemAllocator);

    GOAT_NavigationModuleInterface::GOAT_NavigationModuleInterface()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        // Add ALL components descriptors associated with this gem to m_descriptors.
        // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
        // This happens through the [MyComponent]::Reflect() function.
        m_descriptors.insert(m_descriptors.end(), {
            GOAT_NavigationSystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList GOAT_NavigationModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<GOAT_NavigationSystemComponent>(),
        };
    }
} // namespace GOAT_Navigation
