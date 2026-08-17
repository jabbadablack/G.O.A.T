
#pragma once

namespace GOAT
{
    // System Component TypeIds
    inline constexpr const char* GOATSystemComponentTypeId = "{32E9EAD0-65CE-4BF2-A5A8-D157806321D3}";
    inline constexpr const char* GOATEditorSystemComponentTypeId = "{661F6C2E-6BE3-429D-972D-35A86D7DE02D}";

    // Module derived classes TypeIds
    inline constexpr const char* GOATModuleInterfaceTypeId = "{F1FD652A-0AA6-4E2A-948B-8FE451017576}";
    inline constexpr const char* GOATModuleTypeId = "{91B5D2A0-7A28-416A-8AA5-B0BD8D82BC8B}";
    // The Editor Module by default is mutually exclusive with the Client Module
    // so they use the Same TypeId
    inline constexpr const char* GOATEditorModuleTypeId = GOATModuleTypeId;

    // Interface TypeIds
    inline constexpr const char* GOATRequestsTypeId = "{ADB07321-B3CF-4405-9B88-7D0608C72F34}";
} // namespace GOAT
