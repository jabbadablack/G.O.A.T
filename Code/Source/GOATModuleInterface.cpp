
#include "GOATModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <GOAT/GOATTypeIds.h>

#include <Clients/GOATSystemComponent.h>

namespace GOAT
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(GOATModuleInterface,
        "GOATModuleInterface", GOATModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(GOATModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(GOATModuleInterface, AZ::SystemAllocator);

    GOATModuleInterface::GOATModuleInterface()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        // Add ALL components descriptors associated with this gem to m_descriptors.
        // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
        // This happens through the [MyComponent]::Reflect() function.
        m_descriptors.insert(m_descriptors.end(), {
            GOATSystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList GOATModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<GOATSystemComponent>(),
        };
    }
} // namespace GOAT
