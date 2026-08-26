
#include <GOAT_SmartObject/GOAT_SmartObjectTypeIds.h>
#include <GOAT_SmartObjectModuleInterface.h>
#include "GOAT_SmartObjectSystemComponent.h"

namespace GOAT_SmartObject
{
    class GOAT_SmartObjectModule
        : public GOAT_SmartObjectModuleInterface
    {
    public:
        AZ_RTTI(GOAT_SmartObjectModule, GOAT_SmartObjectModuleTypeId, GOAT_SmartObjectModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_SmartObjectModule, AZ::SystemAllocator);
    };
}// namespace GOAT_SmartObject

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), GOAT_SmartObject::GOAT_SmartObjectModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_SmartObject, GOAT_SmartObject::GOAT_SmartObjectModule)
#endif
