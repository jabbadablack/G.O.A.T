---
type: module
status: implemented
tags: [module, navigation]
---

# Navigation (GOAT_Navigation)

> **Status:** Implemented
> **Gem:** `GOAT_Navigation`
> **Folder:** `Modules/Navigation/`
> **Depends on:** `RecastNavigation`

---

## What it is

Movement verbs. An agent's program says where to go; this gem finds the path and walks it.

---

## The verbs

| Verb | Main property | Does |
| :--- | :--- | :--- |
| `move_to` | `key` | Walks to a position, publishing progress as it goes |
| `is_at_location` | `key` | Succeeds when the agent is already there |
| `does_path_exist` | `key` | Succeeds when a walkable path exists |

All three take a `key` naming a `Vector3` blackboard variable. `move_to` also takes `tolerance`
(how close counts as arrived) and `speed`.

```lua
sequence {
    does_path_exist "move_target",
    move_to { key = "move_target", tolerance = 1.0, speed = 3.5 },
}
```

Checking `does_path_exist` first is the useful pattern: it fails the branch immediately rather
than having the agent set off toward somewhere it cannot reach.

---

## What it publishes

| Variable | Holds |
| :--- | :--- |
| `nav_waypoint` | the next point on the path |
| `nav_steer` | the direction to steer |
| `nav_remaining` | distance left to walk |

These are ordinary blackboard variables, so anything can read them — an animation behaviour
picking a gait, a condition that gives up when a route gets too long.

> **`move_to` returns SUCCESS when it arrives.** You do not need to poll `nav_remaining` in a
> service to find out. That pattern existed before arrival was reported properly and should not
> come back.

---

## The nav mesh

Add a **GOAT Nav Mesh** component to the entity carrying your `RecastNavigationMeshComponent`. It
binds the two together and kicks off the first mesh build on the first tick — deliberately not in
`Activate`, because other entities may not be active yet.

---

## What's inside

| Piece | Does |
| :--- | :--- |
| `NavigationService` | async path requests against the Recast mesh |
| `PathPool` | recycles path buffers so a request allocates nothing after warm-up |
| `Locomotion` | turns a path into per-frame movement |
| `NavigationTarget` | resolves what a `key` is pointing at |
| `SpatialChecks` | the cheap geometric questions |

Paths are requested asynchronously and collected on a later tick. Nothing blocks.

---

## Related

- [[Smart Objects]] — `so_anchor` feeds straight into `move_to`
- [[Adding New Actions]]
- [[Extensibility Model]]

---

*Last updated: 2026-08-27*
