---
type: component
status: active
tags: [cpp, core, runtime, memory]
---

# AgentStore

> **Header:** `Code/Source/Core/Application/AgentStore.h`
> **Source:** `Code/Source/Core/Application/AgentStore.cpp`

---

## Overview

Every agent, addressed by the slot its handle carries.

A slot's index **never changes while the agent lives**. That is the whole point: it lets any
other per-agent table be a plain array indexed by that one number, instead of another hash map
keyed by the same handle.

A released slot becomes a hole rather than being compacted away — compacting is exactly what
would invalidate every index at once. So walking the store means skipping holes:

```cpp
const size_t slotCount = store.GetSlotCount();
for (size_t slot = 0; slot < slotCount; ++slot)
{
    const AgentId candidate = store.GetAgentAtSlot(slot);
    if (candidate.IsNull()) { continue; }   // a hole, not the end
    ...
}
```

[[DirectorRegistry]] resolves a reach exactly this way.

---

## Stable addresses

Records live by value in **chunks that are never resized**, so a record's address never moves
once handed out.

That matters because a tick holds one record for the whole of it. A behaviour that registers a
new agent part way through must not pull the record out from under the tick running it.
[[PlanStore]] hands out spans from fixed chunks for the same reason.

---

## Handles, not pointers

Slots are addressed through [[AgentId]], a generation-checked [[Handle]]. When a slot is reused,
its generation moves on, so a stale handle to a released agent resolves to `nullptr` rather than
silently aliasing whoever took the slot next.

---

## Renamed from

`AgentStore`. It was never a general container — it is the agent store specifically, and the
generic parts live in [[Handle]].

---

## Related

- [[AgentRegistry]]
- [[AgentRecord]]
- [[AgentId]]
- [[Handle]]
- [[PlanStore]]
- [[Performance Model]]

---

*Last updated: 2026-08-27*
