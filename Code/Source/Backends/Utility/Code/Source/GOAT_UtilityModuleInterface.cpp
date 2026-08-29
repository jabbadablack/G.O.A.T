#include <GOAT_UtilityModuleInterface.h>

#include <Clients/GOAT_UtilitySystemComponent.h>

#include <GOAT_Utility/GOAT_UtilityTypeIds.h>

namespace GOAT_Utility
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(GOAT_UtilityModuleInterface,
        "GOAT_UtilityModuleInterface", GOAT_UtilityModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(GOAT_UtilityModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(GOAT_UtilityModuleInterface, AZ::SystemAllocator);

    GOAT_UtilityModuleInterface::GOAT_UtilityModuleInterface()
    {
        m_descriptors.insert(m_descriptors.end(), {
            GOAT_UtilitySystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList GOAT_UtilityModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<GOAT_UtilitySystemComponent>(),
        };
    }
} // namespace GOAT_Utility
