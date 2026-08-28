---
type: component
status: active
tags: [cpp, core, component, director]
---

# GOATDirectorComponent

> **Header:** `Code/Source/Clients/GOATDirectorComponent.h`
> **Source:** `Code/Source/Clients/GOATDirectorComponent.cpp`
> **Inherits:** `AZ::Component`

---

## Overview

Turns an entity into a **director** — an agent whose leaves act on other agents rather than on
itself. It bootstraps exactly like [[GOATAgentComponent]] does, then registers the resulting
handle as a director on top.

It holds nothing about *who* it governs. A director reaches every other agent until a filter
component beside it narrows that. See [[IDirectorFilter]].

---

## Serialized fields

| Field | Type | Description |
| :--- | :--- | :--- |
| `Blackboards` | `vector<Asset<BlackboardAsset>>` | `.bbx` assets declaring the variables its program uses |
| `Scripts` | `vector<Asset<ScriptAsset>>` | Lua declaring its behaviours and programs |
| `Brain` | `string` | Which backend decides. `tree` or `htn`. |
| `Programs` | `vector<string>` | Programs it may run; the first is where it starts |
| `Priority` | `int` (0–255) | Higher outranks lower when two directors command the same agent |
| `Cooldown` | `float` | Seconds before it may command the same agent the same way again |
| `Detail` | `int` (0–3) | Pacing band. 3 by default, which is once a second. |

There is deliberately no reach on this component. It was removed in favour of filter components;
`Squad`, `Tree`, `Radius` and `Filter` no longer exist.

---

## Services

```cpp
provided:     GOATDirectorService, GOATAgentService
incompatible: GOATDirectorService, GOATAgentService
```

It provides `GOATAgentService` because a director **is** an agent. That is also what stops it
sitting beside a [[GOATAgentComponent]], which declares itself incompatible with the same
service — the entity would otherwise be registered twice.

Filter components require `GOATDirectorService`, which is what guarantees this component
activates before them and deactivates after them.

---

## Public interface

```cpp
//! The agent this component registered, or a null handle when it is not running.
AgentId GetAgentId() const;
```

Filter components call this in `Activate` to find the director they attach to.

---

## Lifecycle

**Activate** bootstraps the agent, then builds a [[DirectorProfile]] from `Priority` and
`Cooldown` and calls `IAgentSystem::RegisterDirector`.

**Deactivate** unregisters the director *before* the agent, because the director record is keyed
by the agent handle that the second call releases.

---

## Related

- [[Director AI]]
- [[IDirectorFilter]]
- [[DirectorProfile]]
- [[GOATAgentComponent]]
- [[IAgentSystem]]

---

*Last updated: 2026-08-27*
