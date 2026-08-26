
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
    // Domain value types
    inline constexpr const char* BlackboardKeyTypeId = "{A9DC5B9A-B0EB-4841-9964-483AC10DDBA1}";
    inline constexpr const char* ActionRequestTypeId = "{3B1E1CED-FF55-4E22-A37C-2A0420EE5278}";
    inline constexpr const char* IntentTypeId = "{BB8CF613-4C90-4F49-8021-18EF35DC81CF}";
    inline constexpr const char* ActionPlanTypeId = "{EBA46409-BA3A-46EE-BEBE-5B684AD9317F}";
    inline constexpr const char* GuardTypeId = "{A6264B60-6C2C-46D7-899D-411F03EFF1A7}";
    inline constexpr const char* AgentStateMachineTypeId = "{B5E297DA-3E67-4805-87E7-1339EEAF0632}";

    // Asset TypeIds
    inline constexpr const char* BlackboardVariableTypeId = "{F6CFC1F4-AA6B-47DF-A8B4-19EA46F928E0}";
    inline constexpr const char* BlackboardAssetTypeId = "{29F4C31E-D14F-4DB2-8DBE-B13E59D1ED50}";
    inline constexpr const char* BehaviorTreeAssetTypeId = "{471B8D29-2A2E-4E77-8C33-B8E054EDCA00}";

    // Extension interface TypeIds
    inline constexpr const char* IBackendTypeId = "{369A6D06-454A-4E15-9DBC-B86406CDD9E5}";
    inline constexpr const char* IActionStateTypeId = "{CFC2D26D-0EB8-45B6-9333-93BF41289396}";
    inline constexpr const char* IBlackboardSystemTypeId = "{5B0B29DA-8F97-4C68-8273-74412E3EBFE2}";
    inline constexpr const char* IAgentSystemTypeId = "{363A84A2-B220-4ECA-BD6E-A606A6B41C90}";

    // Component TypeIds
    inline constexpr const char* GOATAgentComponentTypeId = "{60C89039-4265-4A0D-B3ED-75F7C272F371}";
} // namespace GOAT
