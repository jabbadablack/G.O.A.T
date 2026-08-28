---
type: component
status: active
tags: [cpp, core, runtime, memory]
---

# AgentArchetype

> **Header:** `Code/Source/Core/Application/AgentArchetype.h`
> **Source:** `Code/Source/Core/Application/AgentArchetype.cpp`

---

## Overview

Everything a group of identically authored agents shares.

Two entities that list the same programs get the **same archetype** and hold one copy of it
between them. Ten thousand agents of one kind cost one list of programs, not ten thousand.

An agent then remembers a program by its **slot** here, which is why switching programs is a
single byte:

```cpp
using TreeSlot = AZ::u8;
inline constexpr TreeSlot InvalidTreeSlot = static_cast<TreeSlot>(-1);
inline constexpr size_t MaxArchetypeTrees = 32;
```

"May this agent run that program?" becomes a lookup in shared memory instead of a scan over a
per-agent copy of the same names.

---

## Declared before compiled

```cpp
void Add(const AZ::Name& name, AZStd::shared_ptr<const AgentProgram> program);
bool Resolve(const AZ::Name& name, AZStd::shared_ptr<const AgentProgram> program);
```

`Add` accepts a **null** program on purpose. A program an entity declared is part of what
identifies the archetype whether or not it has compiled yet, so the slot is taken now and filled
in by `Resolve` when the program arrives.

Leaving the name out instead would leave the archetype describing a shorter list than the one it
was built from — and the next identically authored agent would fail to match it and build a
second archetype for no reason.

`Resolve` only ever fills an **empty** slot. A program an agent is already running is never
swapped underneath it, and a slot's meaning never changes once handed out.

---

## Sharing

```cpp
bool Matches(AZStd::span<const AZ::Name> names) const;
```

True when this archetype declares exactly these programs, in this order. That is the test for
whether a new agent can share an existing archetype rather than building another.

Order matters because slot zero is where agents of this kind start.

---

## Related

- [[AgentProgram]]
- [[AgentRegistry]]
- [[AgentRecord]]

---

*Last updated: 2026-08-27*
