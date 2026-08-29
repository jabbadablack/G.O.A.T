#include <GOAT_UtilityModuleInterface.h>

#include <GOAT_Utility/GOAT_UtilityTypeIds.h>

namespace GOAT_Utility
{
    class GOAT_UtilityModule
        : public GOAT_UtilityModuleInterface
    {
    public:
        AZ_RTTI(GOAT_UtilityModule, GOAT_UtilityModuleTypeId, GOAT_UtilityModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_UtilityModule, AZ::SystemAllocator);
    };
} // namespace GOAT_Utility

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), GOAT_Utility::GOAT_UtilityModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_Utility, GOAT_Utility::GOAT_UtilityModule)
#endif
