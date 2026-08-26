
#include <GOAT_Animation/GOAT_AnimationTypeIds.h>
#include <GOAT_AnimationModuleInterface.h>
#include "GOAT_AnimationSystemComponent.h"

namespace GOAT_Animation
{
    class GOAT_AnimationModule
        : public GOAT_AnimationModuleInterface
    {
    public:
        AZ_RTTI(GOAT_AnimationModule, GOAT_AnimationModuleTypeId, GOAT_AnimationModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_AnimationModule, AZ::SystemAllocator);
    };
}// namespace GOAT_Animation

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), GOAT_Animation::GOAT_AnimationModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_Animation, GOAT_Animation::GOAT_AnimationModule)
#endif
