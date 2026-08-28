---
type: component
status: active
tags: [cpp, core, registry, director]
---

# DirectorRegistry

> **Header:** `Code/Source/Core/Director/DirectorRegistry.h`
> **Source:** `Code/Source/Core/Director/DirectorRegistry.cpp`

---

## Overview

Which directors exist, who each one governs, and when it may command them again.

Keyed by the director's **own** `AgentId`, because a director is an agent: a verb running on its
program already holds that handle in its `ActionContext`, so looking itself up costs nothing. The
handle is generation-checked, so a stale director can never alias a new one.

```cpp
explicit DirectorRegistry(AgentRegistry& agents);
```

One dependency. It reads no blackboard and computes no geometry — both moved out to filters.

---

## Resolving a reach

```cpp
const AZStd::vector<AgentId>& Resolve(AgentId director);
```

`Evaluate` walks every slot in the [[AgentStore]] and keeps each agent that is not the director
itself and that every attached filter accepts:

```cpp
bool governed = true;
for (const IDirectorFilter* filter : record.m_filters)
{
    if (!filter->Accepts(candidate, agent->m_entity)) { governed = false; break; }
}
```

That is the whole rule. Filters combine with AND; a director with none governs the level.

**A director never governs itself.** It could otherwise order itself onto another program, which
is a loop with no way out.

---

## The cache

The result is cached and re-resolved once per **director tick** — the staleness budget is the
director's own band interval, which is what makes the cache self-managing with no hook to
invalidate it.

Every verb and every sensing call inside one tick must see the same set. A set that changed
underneath a Lua loop would let it address an agent a later verb no longer governs.

Attaching or detaching a filter clears the cache immediately, so a filter added mid-band applies
on the very next resolve rather than a tick later.

---

## Filters

```cpp
bool AttachFilter(AgentId director, IDirectorFilter& filter);
void DetachFilter(AgentId director, IDirectorFilter& filter);
AZStd::vector<const IDirectorFilter*> GetFilters(AgentId director) const;
```

Not owned — see [[IDirectorFilter]]. Attaching the same filter twice is refused.
`GetFilters` exists for the console, which names them with `RTTI_GetTypeName()`.

---

## Cooldowns

```cpp
bool IsOffCooldown(AgentId director, AgentId agent, ActionStateId verb) const;
void StartCooldown(AgentId director, AgentId agent, ActionStateId verb);
```

Keyed by **agent and verb**, and held per director. A cooldown is *this* director's relationship
with that agent: another director must still be able to command it, or one director's order would
silence another's by accident rather than by priority.

`IsOffCooldown` is only the timer. Whether a command would change anything is the verb's own
question, and it has to be asked first so a no-op neither consumes nor starts a cooldown.

Dead entries are swept while `Resolve` is already walking the roster — a cooldown for an agent
that no longer exists can never be asked about again, so it would otherwise sit there for the
level's lifetime.

---

## Related

- [[Director AI]]
- [[IDirectorFilter]]
- [[DirectorProfile]]
- [[GOATDirectorComponent]]
- [[AgentStore]]

---

*Last updated: 2026-08-27*
