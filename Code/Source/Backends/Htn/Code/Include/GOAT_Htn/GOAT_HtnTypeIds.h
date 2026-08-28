#pragma once

namespace GOAT_Htn
{
    inline constexpr const char* GOAT_HtnSystemComponentTypeId = "{BC1EC278-73D9-484B-A640-F0A8843251F9}";
    inline constexpr const char* GOAT_HtnModuleInterfaceTypeId = "{A3EE7B5D-044F-4A76-BDBE-AF3422FF5C51}";
    inline constexpr const char* GOAT_HtnModuleTypeId = "{CEBAEBDE-A3AA-43B8-878B-BD15ACC16592}";
    // The editor module is mutually exclusive with the client one, so it shares its id.
    inline constexpr const char* GOAT_HtnEditorModuleTypeId = GOAT_HtnModuleTypeId;
    inline constexpr const char* GOAT_HtnEditorSystemComponentTypeId = "{7C29E58B-F367-4E29-A982-FB2978B53BCF}";
} // namespace GOAT_Htn
