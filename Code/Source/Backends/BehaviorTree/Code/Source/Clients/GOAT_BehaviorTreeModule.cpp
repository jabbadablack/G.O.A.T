#include <GOAT_BehaviorTreeModuleInterface.h>

#include <GOAT_BehaviorTree/GOAT_BehaviorTreeTypeIds.h>

namespace GOAT_BehaviorTree
{
    class GOAT_BehaviorTreeModule
        : public GOAT_BehaviorTreeModuleInterface
    {
    public:
        AZ_RTTI(GOAT_BehaviorTreeModule, GOAT_BehaviorTreeModuleTypeId, GOAT_BehaviorTreeModuleInterface);
        AZ_CLASS_ALLOCATOR(GOAT_BehaviorTreeModule, AZ::SystemAllocator);
    };
} // namespace GOAT_BehaviorTree

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), GOAT_BehaviorTree::GOAT_BehaviorTreeModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_GOAT_BehaviorTree, GOAT_BehaviorTree::GOAT_BehaviorTreeModule)
#endif
