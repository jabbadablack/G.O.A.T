
#pragma once

namespace GOAT_Navigation
{
    // System Component TypeIds
    inline constexpr const char* GOAT_NavigationSystemComponentTypeId = "{0EEB3FA5-110B-4287-81F8-AC0F1CFF6D06}";
    inline constexpr const char* GOAT_NavigationEditorSystemComponentTypeId = "{C6D74DED-2C16-486B-86DC-57E42AE1788B}";

    // Module derived classes TypeIds
    inline constexpr const char* GOAT_NavigationModuleInterfaceTypeId = "{DC2103A7-F8D2-43BD-BE82-530D92EB5A59}";
    inline constexpr const char* GOAT_NavigationModuleTypeId = "{FD3B5C92-3266-42C3-BA9C-CA317D0EB88F}";
    // The Editor Module by default is mutually exclusive with the Client Module
    // so they use the Same TypeId
    inline constexpr const char* GOAT_NavigationEditorModuleTypeId = GOAT_NavigationModuleTypeId;

    // Interface TypeIds
    inline constexpr const char* GOAT_NavigationRequestsTypeId = "{E4DE1C48-8EBA-42CA-BA8A-7BC22C42019C}";

    // Navigation component TypeIds
    inline constexpr const char* GOATNavMeshComponentTypeId = "{E9D2A791-4981-4B3C-93B3-62CA6A771669}";
} // namespace GOAT_Navigation
