---
type: component
status: active
tags: [cpp, core, component, director, squad]
---

# GOATDirectorSquadFilterComponent

> **Header:** `Code/Source/Clients/GOATDirectorSquadFilterComponent.h`
> **Source:** `Code/Source/Clients/GOATDirectorSquadFilterComponent.cpp`
> **Inherits:** `AZ::Component`, `IDirectorFilter`
> **Shown in the editor as:** GOAT Director Squad

---

## Overview

Narrows a director to the agents in named squads or carrying named tags.

The two lists **union**. `Squads: [Alpha]` with `Tags: [wounded]` governs everyone in Alpha, plus
anyone tagged wounded whatever squad they are in.

---

## Serialized fields

| Field | Type | Description |
| :--- | :--- | :--- |
| `Squads` | `vector<string>` | Squad names, matched against the squad each agent joined |
| `Tags` | `vector<string>` | Tags, read from the agent entity's LmbrCentral **Tag** component |

Both are authored as strings because that is what the property editor can show, and interned once
on `Activate` — to `AZ::Name` and `AZ::Crc32` — because they are compared against every agent on
every director tick.

Leaving both empty narrows nothing and warns. An unfinished component should not silently strip a
director of everyone it governs.

---

## Services

```cpp
provided:     GOATDirectorSquadFilterService
incompatible: GOATDirectorSquadFilterService
required:     GOATDirectorService
```

---

## Tags

Tags are the engine's, not GOAT's. Add the stock LmbrCentral **Tag** component to an agent's
entity and type the tag in; nothing in GOAT has to learn what a tag is. The filter asks
`TagComponentRequestBus::HasTag` on the agent's entity.

Squads are GOAT's own — an agent joins one on [[GOATAgentComponent]], and the filter reads it
back with `IAgentSystem::GetAgentSquad`. An agent is in at most one squad, so "multiple squads"
here means membership in a set.

---

## Related

- [[IDirectorFilter]]
- [[GOATDirectorComponent]]
- [[GOATDirectorAreaFilterComponent]]
- [[SquadRegistry]]
- [[Director AI]]

---

*Last updated: 2026-08-27*
