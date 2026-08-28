#pragma once

namespace GOAT_BehaviorTree
{
    inline constexpr const char* GOAT_BehaviorTreeSystemComponentTypeId = "{0E5C7A31-9B24-4D68-A7F0-52C1D8934E6B}";
    inline constexpr const char* GOAT_BehaviorTreeModuleInterfaceTypeId = "{4A18F6D2-C053-4E97-B21D-7F60958C3A4E}";
    inline constexpr const char* GOAT_BehaviorTreeModuleTypeId = "{93B2E074-5D8A-41C6-9F35-0A47D6182BC9}";
    // The editor module is mutually exclusive with the client one, so it shares its id.
    inline constexpr const char* GOAT_BehaviorTreeEditorModuleTypeId = GOAT_BehaviorTreeModuleTypeId;
    inline constexpr const char* GOAT_BehaviorTreeEditorSystemComponentTypeId = "{6F2A8D14-70B5-4C39-8E1D-B4573A0C9F28}";
} // namespace GOAT_BehaviorTree
