#include <GOAT_HtnModuleInterface.h>

#include <Clients/GOAT_HtnSystemComponent.h>

#include <GOAT_Htn/GOAT_HtnTypeIds.h>

namespace GOAT_Htn
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(GOAT_HtnModuleInterface,
        "GOAT_HtnModuleInterface", GOAT_HtnModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(GOAT_HtnModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(GOAT_HtnModuleInterface, AZ::SystemAllocator);

    GOAT_HtnModuleInterface::GOAT_HtnModuleInterface()
    {
        m_descriptors.insert(m_descriptors.end(), {
            GOAT_HtnSystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList GOAT_HtnModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<GOAT_HtnSystemComponent>(),
        };
    }
} // namespace GOAT_Htn
