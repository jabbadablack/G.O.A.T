#include <GOAT_HtnModuleInterface.h>

#include <GOAT_Htn/GOAT_HtnTypeIds.h>

namespace GOAT_Htn
{
    class GOAT_HtnModule
        : public GOAT_HtnModuleInterface
    {
    public:
        AZ_RTTI(GOAT_HtnModule, GOAT_HtnModuleTypeId, GOAT_HtnModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_HtnModule, AZ::SystemAllocator);
    };
} // namespace GOAT_Htn

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), GOAT_Htn::GOAT_HtnModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_Htn, GOAT_Htn::GOAT_HtnModule)
#endif
