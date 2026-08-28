---
type: component
status: active
tags: [cpp, core, component, director]
---

# GOATDirectorAreaFilterComponent

> **Header:** `Code/Source/Clients/GOATDirectorAreaFilterComponent.h`
> **Source:** `Code/Source/Clients/GOATDirectorAreaFilterComponent.cpp`
> **Inherits:** `AZ::Component`, `IDirectorFilter`
> **Shown in the editor as:** GOAT Director Area

---

## Overview

Narrows a director to the agents standing inside a shape.

The shape is an ordinary O3DE shape component, so a **Sphere Shape** is the plain radius a
director used to author on itself, and a **Box Shape** or **Polygon Prism** is a zone you can
actually draw. Any shape works, because the filter only asks `IsPointInside`.

---

## Serialized fields

None. There is nothing to configure — the shape on the entity is the area.

---

## Services

```cpp
provided:     GOATDirectorAreaFilterService
incompatible: GOATDirectorAreaFilterService
required:     GOATDirectorService, ShapeService
```

Requiring `ShapeService` is what tells an author the component needs a shape, and requiring
`GOATDirectorService` is what guarantees the director has already activated when this attaches.

---

## Pointing it at another entity

```cpp
GOATDirectorAreaFilterRequestBus::Event(
    filterEntity, &GOATDirectorAreaFilterRequests::SetShapeEntity, zoneEntity);
```

Declared in `Code/Include/GOAT/GOATDirectorFilterBus.h`. Passing an invalid id puts it back on
its own entity.

This is code-only on purpose. The common case is the shape sitting right there, and a required
`ShapeService` makes that case correct by construction; an authored field would have given that
up to serve the rarer one. Use this when a zone has to stay put while the director roams, or when
several directors share one authored area.

---

## Behaviour

`Accepts` reads the agent's world translation off `AZ::TransformBus` and asks the shape whether
that point is inside.

It **fails open**: if the shape entity has no `ShapeComponentRequestsBus` handler — missing
component, inactive entity — it accepts everyone and warns once. A broken filter must not quietly
change who is governed. An agent with no transform is accepted for the same reason.

The warning is latched, because `Accepts` runs for every agent in the level on every director
tick.

---

## Related

- [[IDirectorFilter]]
- [[GOATDirectorComponent]]
- [[GOATDirectorSquadFilterComponent]]
- [[Director AI]]

---

*Last updated: 2026-08-27*
