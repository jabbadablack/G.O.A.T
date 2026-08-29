
#include <GOAT_Utility/GOAT_UtilityTypeIds.h>
#include <GOAT_UtilityModuleInterface.h>
#include "GOAT_UtilityEditorSystemComponent.h"

namespace GOAT_Utility
{
    class GOAT_UtilityEditorModule
        : public GOAT_UtilityModuleInterface
    {
    public:
        AZ_RTTI(GOAT_UtilityEditorModule, GOAT_UtilityEditorModuleTypeId, GOAT_UtilityModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_UtilityEditorModule, AZ::SystemAllocator);

        GOAT_UtilityEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                GOAT_UtilityEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<GOAT_UtilityEditorSystemComponent>(),
            };
        }
    };
}// namespace GOAT_Utility

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), GOAT_Utility::GOAT_UtilityEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_Utility_Editor, GOAT_Utility::GOAT_UtilityEditorModule)
#endif
