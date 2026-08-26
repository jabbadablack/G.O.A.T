
#include "GOAT_SmartObjectModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <GOAT_SmartObject/GOAT_SmartObjectTypeIds.h>

#include <Clients/GOAT_SmartObjectSystemComponent.h>
#include <Components/GOATSmartObjectComponent.h>

namespace GOAT_SmartObject
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(GOAT_SmartObjectModuleInterface,
        "GOAT_SmartObjectModuleInterface", GOAT_SmartObjectModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(GOAT_SmartObjectModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(GOAT_SmartObjectModuleInterface, AZ::SystemAllocator);

    GOAT_SmartObjectModuleInterface::GOAT_SmartObjectModuleInterface()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        // Add ALL components descriptors associated with this gem to m_descriptors.
        // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
        // This happens through the [MyComponent]::Reflect() function.
        m_descriptors.insert(m_descriptors.end(), {
            GOAT_SmartObjectSystemComponent::CreateDescriptor(),
            GOATSmartObjectComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList GOAT_SmartObjectModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<GOAT_SmartObjectSystemComponent>(),
        };
    }
} // namespace GOAT_SmartObject
