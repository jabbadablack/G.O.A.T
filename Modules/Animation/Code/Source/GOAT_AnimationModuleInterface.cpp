
#include "GOAT_AnimationModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <GOAT_Animation/GOAT_AnimationTypeIds.h>

#include <Clients/GOAT_AnimationSystemComponent.h>

namespace GOAT_Animation
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(GOAT_AnimationModuleInterface,
        "GOAT_AnimationModuleInterface", GOAT_AnimationModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(GOAT_AnimationModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(GOAT_AnimationModuleInterface, AZ::SystemAllocator);

    GOAT_AnimationModuleInterface::GOAT_AnimationModuleInterface()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        // Add ALL components descriptors associated with this gem to m_descriptors.
        // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
        // This happens through the [MyComponent]::Reflect() function.
        m_descriptors.insert(m_descriptors.end(), {
            GOAT_AnimationSystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList GOAT_AnimationModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<GOAT_AnimationSystemComponent>(),
        };
    }
} // namespace GOAT_Animation
