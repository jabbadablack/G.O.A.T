
#include <GOAT_Navigation/GOAT_NavigationTypeIds.h>
#include <GOAT_NavigationModuleInterface.h>
#include "GOAT_NavigationSystemComponent.h"

namespace GOAT_Navigation
{
    class GOAT_NavigationModule
        : public GOAT_NavigationModuleInterface
    {
    public:
        AZ_RTTI(GOAT_NavigationModule, GOAT_NavigationModuleTypeId, GOAT_NavigationModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_NavigationModule, AZ::SystemAllocator);
    };
}// namespace GOAT_Navigation

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), GOAT_Navigation::GOAT_NavigationModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_Navigation, GOAT_Navigation::GOAT_NavigationModule)
#endif
