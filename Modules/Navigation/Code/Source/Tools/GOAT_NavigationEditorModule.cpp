
#include <GOAT_Navigation/GOAT_NavigationTypeIds.h>
#include <GOAT_NavigationModuleInterface.h>
#include "GOAT_NavigationEditorSystemComponent.h"

namespace GOAT_Navigation
{
    class GOAT_NavigationEditorModule
        : public GOAT_NavigationModuleInterface
    {
    public:
        AZ_RTTI(GOAT_NavigationEditorModule, GOAT_NavigationEditorModuleTypeId, GOAT_NavigationModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_NavigationEditorModule, AZ::SystemAllocator);

        GOAT_NavigationEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                GOAT_NavigationEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<GOAT_NavigationEditorSystemComponent>(),
            };
        }
    };
}// namespace GOAT_Navigation

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), GOAT_Navigation::GOAT_NavigationEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_Navigation_Editor, GOAT_Navigation::GOAT_NavigationEditorModule)
#endif
