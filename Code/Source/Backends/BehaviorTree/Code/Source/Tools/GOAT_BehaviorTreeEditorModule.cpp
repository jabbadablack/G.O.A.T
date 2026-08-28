
#include <GOAT_BehaviorTree/GOAT_BehaviorTreeTypeIds.h>
#include <GOAT_BehaviorTreeModuleInterface.h>
#include "GOAT_BehaviorTreeEditorSystemComponent.h"

namespace GOAT_BehaviorTree
{
    class GOAT_BehaviorTreeEditorModule
        : public GOAT_BehaviorTreeModuleInterface
    {
    public:
        AZ_RTTI(GOAT_BehaviorTreeEditorModule, GOAT_BehaviorTreeEditorModuleTypeId, GOAT_BehaviorTreeModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_BehaviorTreeEditorModule, AZ::SystemAllocator);

        GOAT_BehaviorTreeEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                GOAT_BehaviorTreeEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<GOAT_BehaviorTreeEditorSystemComponent>(),
            };
        }
    };
}// namespace GOAT_BehaviorTree

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), GOAT_BehaviorTree::GOAT_BehaviorTreeEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_BehaviorTree_Editor, GOAT_BehaviorTree::GOAT_BehaviorTreeEditorModule)
#endif
