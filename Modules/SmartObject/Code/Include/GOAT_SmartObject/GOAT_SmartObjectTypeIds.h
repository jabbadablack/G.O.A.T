
#pragma once

namespace GOAT_SmartObject
{
    // System Component TypeIds
    inline constexpr const char* GOAT_SmartObjectSystemComponentTypeId = "{D4CE6666-C742-4FC5-8B38-05D42F4CC104}";
    inline constexpr const char* GOAT_SmartObjectEditorSystemComponentTypeId = "{3EBBFD0A-A269-45EE-8A8B-2CDB4483E162}";

    // Module derived classes TypeIds
    inline constexpr const char* GOAT_SmartObjectModuleInterfaceTypeId = "{F05AF793-2F37-4655-A81A-3B2CC18B99F6}";
    inline constexpr const char* GOAT_SmartObjectModuleTypeId = "{304DD9C1-6541-4865-AEB5-B2275582DB8B}";
    // The Editor Module by default is mutually exclusive with the Client Module
    // so they use the Same TypeId
    inline constexpr const char* GOAT_SmartObjectEditorModuleTypeId = GOAT_SmartObjectModuleTypeId;

    // Interface TypeIds
    inline constexpr const char* GOAT_SmartObjectRequestsTypeId = "{4EEBC79F-2F87-42DF-B980-BA3FAAB19E11}";

    // Smart object component TypeIds
    inline constexpr const char* GOATSmartObjectComponentTypeId = "{7C1F5A9D-3E62-4B18-9D4A-2C8F6B0E7A31}";
} // namespace GOAT_SmartObject
