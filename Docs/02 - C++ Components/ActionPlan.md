---
type: component
status: active
tags: [cpp, core, domain, plan]
---

# ActionPlan

> **Header:** `Code/Include/GOAT/Domain/ActionPlan.h`
> **Kind:** Plain struct, public API

---

## Overview

The sequence of actions a backend produced, and the input to the state machine.

It is a **view into a [[PlanStore]]**, not a buffer of its own:

```cpp
struct ActionPlan final
{
    bool IsEmpty() const;
    size_t Size() const;
    const ActionRequest* GetStep(size_t index) const;
    bool IsBorrowed() const;

    PlanStore::Span m_span;
};
```

Sixteen bytes, whatever the plan contains.

---

## Why it is a span

It used to be a fixed buffer with a hard cap of eight steps. Two things came from making it a
span instead.

**There is no length limit.** A five-hundred-step plan costs an agent exactly the same sixteen
bytes a one-step plan does.

**Nothing is copied.** An authored plan's steps are baked once and shared by every agent running
it, so reaching a plan boundary copies nothing at all.

---

## Ownership

The span stays valid as long as the store that issued it does.

- **Baked** — an authored plan. Never released. `IsBorrowed()` is false.
- **Borrowed** — computed at runtime. Must be given back when the plan ends or is abandoned.

The runtime handles the release. A backend calls `Acquire` and hands the span over; it should not
keep a copy.

---

## Getting one

```cpp
outPlan.m_span = context.m_planStore->Acquire(steps.data(), count);
```

An empty plan is a failure, not a no-op — see [[IDecisionBackend]].

---

## Related

- [[PlanStore]]
- [[ActionRequest]]
- [[AgentStateMachine]]
- [[IDecisionBackend]]

---

*Last updated: 2026-08-27*
