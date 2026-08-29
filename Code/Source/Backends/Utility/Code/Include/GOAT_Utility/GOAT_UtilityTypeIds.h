#pragma once

namespace GOAT_Utility
{
    inline constexpr const char* GOAT_UtilitySystemComponentTypeId = "{35E4D8A4-CBD9-4FC1-B51E-E994D903B9EE}";
    inline constexpr const char* GOAT_UtilityModuleInterfaceTypeId = "{B7E6C02D-BE7B-446C-8A60-4C2AEE2BC375}";
    inline constexpr const char* GOAT_UtilityModuleTypeId = "{02D87C6C-C9F9-439F-B079-CBC0E6EB5902}";
    // The editor module is mutually exclusive with the client one, so it shares its id.
    inline constexpr const char* GOAT_UtilityEditorModuleTypeId = GOAT_UtilityModuleTypeId;
    inline constexpr const char* GOAT_UtilityEditorSystemComponentTypeId = "{4D71BE1C-B58D-46B8-94D9-47BE68BA707D}";
} // namespace GOAT_Utility
