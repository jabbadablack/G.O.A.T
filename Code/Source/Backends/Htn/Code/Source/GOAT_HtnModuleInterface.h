#pragma once

#include <AzCore/Memory/Memory.h>
#include <AzCore/Module/Module.h>

namespace GOAT_Htn
{
    class GOAT_HtnModuleInterface
        : public AZ::Module
    {
    public:
        AZ_TYPE_INFO_WITH_NAME_DECL(GOAT_HtnModuleInterface);
        AZ_RTTI_NO_TYPE_INFO_DECL();
        AZ_CLASS_ALLOCATOR_DECL;

        GOAT_HtnModuleInterface();

        AZ::ComponentTypeList GetRequiredSystemComponents() const override;
    };
} // namespace GOAT_Htn
