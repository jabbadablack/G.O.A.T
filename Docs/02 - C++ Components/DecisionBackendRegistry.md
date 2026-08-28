---
type: component
status: active
tags: [cpp, core, registry, backend]
---

# DecisionBackendRegistry

> **Header:** `Code/Source/Core/Application/DecisionBackendRegistry.h`

---

## Overview

Which paradigms exist, each owned here and found by the name it answers to.

```cpp
using DecisionBackendRegistry = NamedRegistry<IDecisionBackend>;
```

That is the entire file. It is an alias for `NamedRegistry`, a small generic map from
`AZ::Name` to `unique_ptr<T>` that also backs the action and node-type registries.

The noun is passed to the constructor rather than inferred, so its messages still say what failed:

```cpp
DecisionBackendRegistry registry("decision backend");
// -> "decision backend 'bt' is already registered"
```

which is worth more to whoever reads the log than "item 'bt' is".

---

## Why a registry at all

A backend is looked up by name because that is what an agent's **Brain** field holds — a string
an author typed. `tree` and `htn` both register here at startup, from their own gems, and an
agent naming one that no gem installed fails with a message rather than silently doing nothing.

Names must be unique. Registering a taken one fails and says so.

---

## Reached through the bus

Modules never touch the registry directly. They go through `GOATBackendRequestBus`, which is what
lets a paradigm live in a gem that the core knows nothing about:

```cpp
GOATBackendRequestBus::BroadcastResult(
    registered, &GOATBackendRequests::RegisterDecisionBackend, backend);
```

---

## Related

- [[IDecisionBackend]]
- [[BackendRegistry]]
- [[Extensibility Model]]

---

*Last updated: 2026-08-27*
