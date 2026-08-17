
#include <GOAT/GOATTypeIds.h>
#include <GOATModuleInterface.h>
#include "GOATSystemComponent.h"

namespace GOAT
{
    class GOATModule
        : public GOATModuleInterface
    {
    public:
        AZ_RTTI(GOATModule, GOATModuleTypeId, GOATModuleInterface);
        AZ_CLASS_ALLOCATOR(GOATModule, AZ::SystemAllocator);
    };
}// namespace GOAT

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), GOAT::GOATModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT, GOAT::GOATModule)
#endif
