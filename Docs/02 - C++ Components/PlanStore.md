---
type: component
status: active
tags: [cpp, core, domain, memory, performance]
---

# PlanStore

> **Header:** `Code/Include/GOAT/Domain/PlanStore.h`
> **Source:** `Code/Source/Core/Domain/PlanStore.cpp`

---

## Overview

Where the steps of every plan live.

An agent's plan is a **span into this**, not a copy. That removes two things at once: the length
limit a fixed plan buffer imposes, and the copy every plan used to pay.

```cpp
struct Span final
{
    const ActionRequest* m_steps = nullptr;
    AZ::u32 m_count = 0;
    PlanBlockId m_block = InvalidPlanBlock;  // non-zero only when borrowed
};
```

[[ActionPlan]] *is* one of these spans.

---

## Two regions

Authored and computed plans have nothing in common but their element type, so they are kept apart.

**Baked.** Every option of every authored `plan`, written once when the vocabulary loads and
immutable afterwards. Nothing is copied at runtime and nothing is ever released.

**Pooled.** Steps a backend computed while the game ran. Borrowed when the plan starts, returned
when it ends or is abandoned. Blocks are recycled rather than freed, so after warm-up a computed
plan allocates nothing at all.

```cpp
Span Bake(const ActionRequest* steps, AZ::u32 count);      // authored, never released
Span Acquire(const ActionRequest* steps, AZ::u32 count);   // computed, must be released
void Release(Span span);                                    // ignores a baked span
size_t GetBorrowedCount() const;                            // proves plans are given back
```

`Release` quietly ignores a baked span, so the caller does not have to know which kind it holds.

---

## Pointer stability is the whole design

Both regions hand out raw pointers that must stay valid for as long as an agent is running that
plan. So **neither region ever grows a buffer that is already in use** — a chunk's capacity is
decided when it is created and never revisited.

A plan longer than `BakedChunkSteps` (256) gets a chunk of its own, so a span is always
contiguous however long the plan is.

[[AgentStore]] keeps agent records in fixed chunks for exactly the same reason.

---

## Checking it

`GetBorrowedCount()` is the leak check: it should return to zero once agents stop running
computed plans. `ClearBaked` requires aborting any agent still on a baked plan first, because
their spans point into the memory it releases.

---

## Related

- [[ActionPlan]]
- [[ActionRequest]]
- [[PlanContext]]
- [[AgentStore]]
- [[Performance Model]]

---

*Last updated: 2026-08-27*
