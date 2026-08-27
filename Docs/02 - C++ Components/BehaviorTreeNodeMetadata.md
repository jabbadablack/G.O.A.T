---
type: component
status: active
tags: [cpp, core, domain]
---

# BehaviorTreeNodeMetadata

> **File Location:** `Code/Include/GOAT/Assets/BehaviorTreeAsset.h`  
> **Source:** `Code/Source/Core/Assets/BehaviorTreeAsset.cpp`  
> **Inherits:** None (Plain struct)

---

## Overview

`BehaviorTreeNodeMetadata` stores **editor-only data** for a `BehaviorTreeNode`. It is used by a future graph editor to round-trip node positioning and comments. The runtime never reads it, but it is serialized with the node for editing purposes.

---

## Key Responsibilities

| # | Responsibility | Description |
| :--- | :--- | :--- |
| 1 | **Position Storage** | Stores the node's position on a graph canvas. |
| 2 | **Comment Storage** | Stores an author's note about the node. |
| 3 | **Editor Round-Trip** | Allows a graph tool to save and load node layout without affecting runtime. |

---

## Public Interface

### Data Members

| Member | Type | Description |
| :--- | :--- | :--- |
| `m_position` | `AZ::Vector2` | Where the node sits on a graph canvas. |
| `m_comment` | `AZStd::string` | Author's note about the node. |

---

## Dependencies & Interactions

```mermaid
graph LR
    A[BehaviorTreeNodeMetadata] --> B[BehaviorTreeNode]
    B --> C[TreeCompiler]
```

- **Depends on:** `AZ::Vector2`, `AZStd::string`.
- **Required by:** `BehaviorTreeNode` (for editor integration).

---

## Implementation Notes

### Key Algorithms

`BehaviorTreeNodeMetadata` is purely serialized. `TreeCompiler` ignores it entirely when compiling.

### Performance Considerations

- **Allocation:** No runtime allocation.
- **Tick Rate:** Not used during runtime.
- **Concurrency:** Main thread only.

---

## Lua Exposure

Not directly exposed to Lua. Only used by a future graph editor.

---

## Testing

Unit tests should cover:

- **Position:** Correctly stores and retrieves `AZ::Vector2`.
- **Comment:** Correctly stores and retrieves a string.

---

## Related Notes

- [[BehaviorTreeNode]]
- [[BehaviorTreeProperty]]
- [[BehaviorTreeAsset]]

---

*Last updated: 2026-08-26*