---
type: component
status: active
tags: [cpp, core, domain]
---

# Intent

> **File Location:** `Code/Include/GOAT/Domain/Intent.h`  
> **Source:** `Code/Source/Core/Domain/Intent.cpp`  
> **Inherits:** None (Plain struct)

---

## Overview

`Intent` is the **message a leaf sends when it wants something done**. The walker builds one when it reaches an `Action`, `Script` or `Delegate` node.

What happens next depends on `m_backend`. **Empty means the leaf is the whole plan already** — the tree backend acquires a one-step plan from the [[PlanStore]] and runs it, with no round trip through anything. A name means a [[IBackend]] is asked to satisfy it, which is how a `delegate` reaches a Lua planner.

It contains either a direct action (from `raw` or `script` leaves) or a backend name plus a goal (from `delegate` leaves).

---

## Key Responsibilities

| # | Responsibility | Description |
| :--- | :--- | :--- |
| 1 | **Backend Selection** | Names the backend that should satisfy this intent (`m_backend`). |
| 2 | **Goal Specification** | Provides a goal for planning backends (`m_goal`). |
| 3 | **Inline leaf** | An empty `m_backend`, satisfied straight from the node's own `ActionRequest`. |
| 4 | **Source Tracking** | Records which tree node produced this intent (`m_node`) for debugging and resuming. |

---

## Public Interface

### Data Members

| Member | Type | Description |
| :--- | :--- | :--- |
| `m_backend` | `AZ::Name` | Backend asked to satisfy this intent. Empty means the leaf is satisfied inline. |
| `m_goal` | `AZ::Name` | What to achieve, interpreted by the backend. A goal name for a planner. |
| `m_direct` | `ActionRequest` | Action the tree authored inline, used by the direct backend as a one-step plan. |
| `m_node` | `NodeIndex` | Tree node this intent came from, for debugging and for resuming the walk. |

### Methods

```cpp
static void Reflect(AZ::ReflectContext* context);
```

---

## Dependencies & Interactions

```mermaid
graph LR
    A[TreeWalker] -->|Produces| B[Intent]
    B --> C[BackendRegistry]
    C --> D[IBackend]
    D --> E[ActionPlan]
    E --> F[AgentStateMachine]
```

- **Depends on:** `ActionRequest`, `AZ::Name`, `NodeIndex`.
- **Required by:** `TreeWalker`, `BackendRegistry`, `AgentRuntime`.

---

## Implementation Notes

### Key Algorithms

`TreeWalker::MakeIntent()` constructs an `Intent` based on the node operation:

```cpp
Intent TreeWalker::MakeIntent(const DecisionNode& node, NodeIndex index) const
{
    Intent intent;
    intent.m_node = index;

    switch (node.m_op)
    {
    case NodeOp::Action:
        intent.m_backend = AZ::Name();  // inline
        intent.m_direct = node.m_action;
        break;
    case NodeOp::Script:
        intent.m_backend = AZ::Name();  // inline
        intent.m_direct.m_action = CoreActions::RunScript;
        intent.m_direct.m_tag = node.m_tag;
        break;
    case NodeOp::Delegate:
        intent.m_backend = node.m_tag;
        intent.m_goal = node.m_goal;
        break;
    default:
        break;
    }

    return intent;
}
```

### Performance Considerations

- **Allocation:** No heap allocations; plain struct.
- **Tick Rate:** Created whenever the walker reaches a leaf node.
- **Concurrency:** Immutable after creation; safe to pass by const reference.

---

## Lua Exposure

Not directly exposed to Lua. Lua trees use `script`, `raw`, and `delegate` nodes, which are converted into `Intent`s by the C++ walker.

---

## Testing

Unit tests should cover:

- **Action Intent:** Correctly sets `m_backend` to "direct" and `m_direct` to the action.
- **Script Intent:** Correctly sets `m_backend` to "direct" and `m_direct.m_action` to RunScript.
- **Delegate Intent:** Correctly sets `m_backend` and `m_goal`.

---

## Related Notes

- [[IBackend]]
- [[TreeWalker]]
- [[ActionRequest]]

---

*Last updated: 2026-08-26*