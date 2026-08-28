
#include <GOAT_Htn/GOAT_HtnTypeIds.h>
#include <GOAT_HtnModuleInterface.h>
#include "GOAT_HtnEditorSystemComponent.h"

namespace GOAT_Htn
{
    class GOAT_HtnEditorModule
        : public GOAT_HtnModuleInterface
    {
    public:
        AZ_RTTI(GOAT_HtnEditorModule, GOAT_HtnEditorModuleTypeId, GOAT_HtnModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_HtnEditorModule, AZ::SystemAllocator);

        GOAT_HtnEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                GOAT_HtnEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<GOAT_HtnEditorSystemComponent>(),
            };
        }
    };
}// namespace GOAT_Htn

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), GOAT_Htn::GOAT_HtnEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_Htn_Editor, GOAT_Htn::GOAT_HtnEditorModule)
#endif
