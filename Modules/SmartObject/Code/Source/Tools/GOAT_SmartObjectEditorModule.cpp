
#include <GOAT_SmartObject/GOAT_SmartObjectTypeIds.h>
#include <GOAT_SmartObjectModuleInterface.h>
#include "GOAT_SmartObjectEditorSystemComponent.h"

namespace GOAT_SmartObject
{
    class GOAT_SmartObjectEditorModule
        : public GOAT_SmartObjectModuleInterface
    {
    public:
        AZ_RTTI(GOAT_SmartObjectEditorModule, GOAT_SmartObjectEditorModuleTypeId, GOAT_SmartObjectModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_SmartObjectEditorModule, AZ::SystemAllocator);

        GOAT_SmartObjectEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                GOAT_SmartObjectEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<GOAT_SmartObjectEditorSystemComponent>(),
            };
        }
    };
}// namespace GOAT_SmartObject

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), GOAT_SmartObject::GOAT_SmartObjectEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_SmartObject_Editor, GOAT_SmartObject::GOAT_SmartObjectEditorModule)
#endif
