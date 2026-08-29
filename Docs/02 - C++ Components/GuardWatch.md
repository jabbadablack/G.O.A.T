---
type: component
status: active
tags: [cpp, core, runtime, reactivity]
---

# GuardWatch

> **Header:** `Code/Source/Core/Application/GuardWatch.h`
> **Source:** `Code/Source/Core/Application/GuardWatch.cpp`
> **Lives in:** [[AgentRecord]], one per agent

---

## Overview

Notices when anything an agent's program guards on has changed — **without subscribing to it**.

This is the piece that makes reactivity cheap. The obvious design is a callback per agent per
watched key, so one global write walks every agent in the level. Instead each blackboard scope
counts its own changes, and each agent remembers the count it last acted on. A write is one
increment. An agent asks "has the count moved?" on its own tick.

```cpp
void Connect(const AgentProgram& program, IBlackboardSystem& blackboard, AgentId agent);
void Disconnect();
bool IsDirty() const;
void MarkDirty();
void Clear();
```

Because it holds no handler and captures no address, the record it lives in is free to move.

---

## The trade

A **scope** is the finest thing counted, not a key. Any change to a scope an agent watches wakes
it, not only a change to the exact slot it guards on.

That is the right trade for an idle agent, which by definition is running nothing that writes.
Going finer would need per-agent baseline state for every key, and the whole `GuardWatch` is 40
bytes inside a 192-byte agent record.

A program with no guards watches nothing and is never woken by a write at all.

---

## When it re-connects

Connecting is not once-and-for-all. It happens again whenever what the agent watches could have
changed underneath it — joining or leaving a squad, or switching to a different program. An agent
that joined a squad without re-connecting would be watching storage that did not exist when it
first connected.

`MarkDirty` covers the cases a blackboard write cannot: the first tick, because a freshly
connected agent has never evaluated its guards at all.

---

## Renamed from

`GuardWatch`. The name was misleading — it observes nothing and registers no observer, which
is the entire point of the design.

---

## Related

- [[GuardEvaluator]]
- [[AgentRecord]]
- [[BlackboardStorage]]
- [[Guard]]
- [[Performance Model]]

---

*Last updated: 2026-08-27*
