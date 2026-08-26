---
type: component
status: active
tags: [cpp, core, domain]
---

# BehaviorTreeProperty

> **File Location:** `Code/Include/GOAT/Assets/BehaviorTreeAsset.h`  
> **Source:** `Code/Source/Core/Assets/BehaviorTreeAsset.cpp`  
> **Inherits:** None (Plain struct)

---

## Overview

`BehaviorTreeProperty` is **one authored property** on a `BehaviorTreeNode`. It stores the property's name and its value as an `AZStd::any`. It is resolved against the node type at compile time by `TreeCompiler`.

It allows authored trees to carry arbitrary typed data (e.g., `key`, `interval`, `abort`, `seconds`) without the runtime needing to know about it ahead of time.

---

## Key Responsibilities

| # | Responsibility | Description |
| :--- | :--- | :--- |
| 1 | **Property Storage** | Stores the property's name and value. |
| 2 | **Type Erasure** | Uses `AZStd::any` to hold values of any type. |
| 3 | **Compilation Input** | Consumed by `TreeCompiler` to validate and resolve properties. |

---

## Public Interface

### Data Members

| Member | Type | Description |
| :--- | :--- | :--- |
| `m_name` | `AZStd::string` | The property's name. |
| `m_value` | `AZStd::any` | The property's value. |

---

## Dependencies & Interactions

```mermaid
graph LR
    A[BehaviorTreeProperty] --> B[BehaviorTreeNode]
    B --> C[TreeCompiler]
    C --> D[DecisionNode]
```

- **Depends on:** `AZStd::string`, `AZStd::any`.
- **Required by:** `BehaviorTreeNode`, `TreeCompiler`.

---

## Implementation Notes

### Key Algorithms

`TreeCompiler` iterates through a node's properties and matches them against the `NodeTypeDescriptor`'s parameters. It converts values from `AZStd::any` to the expected C++ types (e.g., `AZ::Name`, `float`, `bool`).

### Performance Considerations

- **Allocation:** `AZStd::any` may allocate for larger types, but properties are authored once at load time.
- **Tick Rate:** Not used during runtime.
- **Concurrency:** Main thread only.

---

## Lua Exposure

Indirectly produced from Lua via `LuaTreeBuilder`:

```lua
builder:SetStringProperty("behavior", "Patrol")
builder:SetNumberProperty("seconds", 0.5)
builder:SetBoolProperty("enabled", true)
```

---

## Testing

Unit tests should cover:

- **Property Storage:** Correctly stores name and value.
- **Type Erasure:** Correctly holds values of different types.

---

## Related Notes

- [[BehaviorTreeNode]]
- [[TreeCompiler]]
- [[LuaTreeBuilder]]

---

*Last updated: 2026-08-26*