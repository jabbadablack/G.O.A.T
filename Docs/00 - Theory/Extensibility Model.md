---
type: theory
status: implemented
tags: [architecture, design, extensibility]
---

# Extensibility Model

> **Status:** Implemented
> **Core files:** `Code/Include/GOAT/Interfaces/IAgentSystem.h`, `Code/Include/GOAT/VocabularyScope.h`, `Code/Include/GOAT/GOATBackendBus.h`

---

## The idea

**You never edit the core to add behaviour.** Everything is added from outside, by a gem, through
one interface.

That is not aspiration — it is checkable. Both AI paradigms were lifted out of the core into their
own gems without the core gaining a single reference to either. If a project deletes
`GOAT_BehaviorTree`, the core still builds and task networks still run.

---

## Five hook points

| Add | By implementing | Registered with |
| :--- | :--- | :--- |
| a **verb** — `move_to`, `play_anim` | `IActionState` | `RegisterAction` |
| a **paradigm** — trees, task networks | [[IDecisionBackend]] | `GOATBackendRequestBus` |
| a **word** the DSL understands | `NodeTypeDescriptor` | `RegisterNodeType` |
| a **way to narrow a director** | [[IDirectorFilter]] | `AttachDirectorFilter` |
| a **planner behind `delegate`** | [[IBackend]] | `RegisterBackend` |

Plus one that is not an interface at all: `RegisterVocabularyScript` installs a Lua file, so a gem
ships the words it brings rather than having them hardcoded in `GOAT.lua`.

---

## The façade

[[IAgentSystem]] is the whole surface a gem gets. Reached through an `AZ::Interface`:

```cpp
IAgentSystem* agents = AgentSystemInterface::Get();
```

It is deliberately not the internals. A gem gets to register things, look words up, ask about
agents, and call a behaviour — and that is the list. It cannot reach the registries directly, the
state machine, or another gem's types.

---

## What a gem's startup looks like

Every gem does the same four things, and none of them are special-cased in the core:

```cpp
// 1. a verb, and the word that runs it
const ActionStateId id = agents->RegisterAction(AZStd::make_unique<MoveToAction>(...));
agents->RegisterNodeType(AZStd::move(descriptor));

// 2. a paradigm
GOATBackendRequestBus::BroadcastResult(
    ok, &GOATBackendRequests::RegisterDecisionBackend, backend);

// 3. the Lua file declaring its words
agents->RegisterVocabularyScript("goat_htn/scripts/htn");

// 4. remember it all, so it can be taken back out
m_vocabulary.Own(id);
```

---

## Taking it back out

`VocabularyScope` remembers what a gem installed and unwinds it in reverse order on shutdown:

```cpp
void Clear()
{
    IAgentSystem* agents = AgentSystemInterface::Get();
    if (agents == nullptr)
    {
        // The core shut down first, which takes its registries with it.
        m_nodeTypes.clear();
        m_actions.clear();
        return;
    }
    ...
}
```

The null check matters. Shutdown order is not guaranteed, and a gem must not try to unregister
from a core that is already gone.

---

## Registering is fallible, and says why

Names are unique, and a clash is reported rather than silently overwritten:

```
decision backend 'bt' is already registered
behaviour name 'Patrol' is already taken
```

`NamedRegistry` takes the noun as a constructor argument for exactly this — "backend 'bt' is
already registered" tells whoever reads the log more than "item 'bt' is".

---

## Where the gems live

```
Code/Source/Backends/BehaviorTree/     a paradigm
Code/Source/Backends/Htn/              a paradigm
Modules/Navigation/                    verbs
Modules/Animation/                     verbs
Modules/SmartObject/                   verbs
```

Backends sit under `Code/Source/Backends/` rather than `Modules/` because they are paradigms
rather than domain vocabularies, but structurally they are identical: a `gem.json`, a
`CMakeLists.txt`, a `Platform/` folder and an asset scan folder, all listed in the root
`gem.json`'s `external_subdirectories`.

A gem with assets needs an Editor variant too, or the Asset Processor never sees its scan folder.

---

## What extension does not get you

Some things are deliberately closed:

- **No reaching the state machine.** A backend says `Continue` or `Abandon`. See
  [[Backend Abstraction Theory]].
- **No new blackboard scopes.** Global, Agent and Squad, and the epoch design depends on there
  being few of them.
- **No per-key wakeups.** Reactivity is per scope. Going finer needs per-agent baseline state for
  every key; measure before asking for it.

---

## Related

- [[IAgentSystem]]
- [[IDecisionBackend]]
- [[IDirectorFilter]]
- [[Backend Abstraction Theory]]
- [[Layered Overview]]
- [[Adding New Actions]]

---

*Last updated: 2026-08-27*
