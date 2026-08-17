
#include <GOAT/GOATTypeIds.h>
#include <GOATModuleInterface.h>
#include "GOATEditorSystemComponent.h"

namespace GOAT
{
    class GOATEditorModule
        : public GOATModuleInterface
    {
    public:
        AZ_RTTI(GOATEditorModule, GOATEditorModuleTypeId, GOATModuleInterface);
        AZ_CLASS_ALLOCATOR(GOATEditorModule, AZ::SystemAllocator);

        GOATEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                GOATEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<GOATEditorSystemComponent>(),
            };
        }
    };
}// namespace GOAT

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), GOAT::GOATEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_Editor, GOAT::GOATEditorModule)
#endif
