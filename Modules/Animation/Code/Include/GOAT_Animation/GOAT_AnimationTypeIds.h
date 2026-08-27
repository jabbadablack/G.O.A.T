
#pragma once

namespace GOAT_Animation
{
    // System Component TypeIds
    inline constexpr const char* GOAT_AnimationSystemComponentTypeId = "{EECBC8F5-0861-43F7-A3D7-CF412190F542}";
    inline constexpr const char* GOAT_AnimationEditorSystemComponentTypeId = "{45BAD222-7A3F-45AC-AC00-0C43A90E85B2}";

    // Module derived classes TypeIds
    inline constexpr const char* GOAT_AnimationModuleInterfaceTypeId = "{238B58FE-AEDD-4379-A51C-0623D1162B15}";
    inline constexpr const char* GOAT_AnimationModuleTypeId = "{9549D466-9825-49A9-B7DF-C017537BC46C}";
    // The Editor Module by default is mutually exclusive with the Client Module
    // so they use the Same TypeId
    inline constexpr const char* GOAT_AnimationEditorModuleTypeId = GOAT_AnimationModuleTypeId;

    // Interface TypeIds
    inline constexpr const char* GOAT_AnimationRequestsTypeId = "{9A293B43-2F5C-4D85-B7C2-C8F748915D2E}";
} // namespace GOAT_Animation
