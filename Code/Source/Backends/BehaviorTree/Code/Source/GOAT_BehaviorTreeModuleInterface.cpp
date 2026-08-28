#include <GOAT_BehaviorTreeModuleInterface.h>

#include <Clients/GOAT_BehaviorTreeSystemComponent.h>

#include <GOAT_BehaviorTree/GOAT_BehaviorTreeTypeIds.h>

namespace GOAT_BehaviorTree
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(GOAT_BehaviorTreeModuleInterface,
        "GOAT_BehaviorTreeModuleInterface", GOAT_BehaviorTreeModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(GOAT_BehaviorTreeModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(GOAT_BehaviorTreeModuleInterface, AZ::SystemAllocator);

    GOAT_BehaviorTreeModuleInterface::GOAT_BehaviorTreeModuleInterface()
    {
        m_descriptors.insert(m_descriptors.end(), {
            GOAT_BehaviorTreeSystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList GOAT_BehaviorTreeModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<GOAT_BehaviorTreeSystemComponent>(),
        };
    }
} // namespace GOAT_BehaviorTree
