
#include <GOAT_Animation/GOAT_AnimationTypeIds.h>
#include <GOAT_AnimationModuleInterface.h>
#include "GOAT_AnimationEditorSystemComponent.h"

namespace GOAT_Animation
{
    class GOAT_AnimationEditorModule
        : public GOAT_AnimationModuleInterface
    {
    public:
        AZ_RTTI(GOAT_AnimationEditorModule, GOAT_AnimationEditorModuleTypeId, GOAT_AnimationModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_AnimationEditorModule, AZ::SystemAllocator);

        GOAT_AnimationEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                GOAT_AnimationEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<GOAT_AnimationEditorSystemComponent>(),
            };
        }
    };
}// namespace GOAT_Animation

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), GOAT_Animation::GOAT_AnimationEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_Animation_Editor, GOAT_Animation::GOAT_AnimationEditorModule)
#endif
