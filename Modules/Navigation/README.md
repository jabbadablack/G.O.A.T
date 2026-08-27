# GOAT_Navigation

Spatial behaviour for GOAT agents. Nothing here is in the core gem, so a project that never
needs a navigation mesh does not enable this and never links RecastNavigation.

## What it adds

| Word | Kind | What it does |
|---|---|---|
| `move_to` | leaf | Walks to a position, publishing progress to the blackboard |
| `is_at_location` | leaf | Succeeds when the agent is already within tolerance of a position |
| `does_path_exist` | leaf | Succeeds when a walkable path to a position exists |

Each takes `key` (a Vector3 blackboard variable naming the target), `tolerance`, and `speed`.
Written as `move_to "target"` for the common case, or `move_to { key = "target", speed = 6.0 }`.

It also declares three agent scoped blackboard variables, so no `.bbx` has to mention them:

| Variable | Type | Written by | Meaning |
|---|---|---|---|
| `nav_waypoint` | Vector3 | `move_to` | Next point along the current path |
| `nav_remaining` | Float | `move_to` | Distance left along the current path |
| `nav_steer` | Bool | the project | False when the project moves the entity itself |

**A project with its own character controller sets `nav_steer` to false.** `move_to` then still
publishes `nav_waypoint` and `nav_remaining` every step, but stops moving the transform, so the
project's controller drives movement from those two values.

## Setting up a level

1. On the entity that carries the **Recast Navigation Mesh**, add **GOAT Nav Mesh**. That is the
   whole binding step: RecastNavigation offers no way to look a mesh up, so this component reports
   the entity it is on, which is also the address the navigation mesh notification bus uses.
2. That entity also needs an **Axis Aligned Box Shape** (the volume to voxelize) and a
   **Recast Navigation PhysX Provider**, which are RecastNavigation's own requirements.
3. Give the level some **PhysX static geometry** to walk on. The provider builds the navigation
   mesh from PhysX colliders, so an entity with no collider contributes nothing and the mesh
   comes out empty.
4. On each agent, point **GOAT Agent** at `goat/scripts/navagent.lua` and
   `goat/blackboards/navigation.bbx`, with tree name `NavAgent`.

## How queries run

RecastNavigation hands out a single `dtNavMeshQuery` behind one exclusive mutex, and a
`dtNavMeshQuery` carries mutable node pools, so sharing it would serialise every query no matter
how many threads asked. `NavigationService` therefore borrows only the `dtNavMesh` and gives each
worker its own query object, guarding reads with its own shared mutex whose write window is
`OnNavigationMeshBeganRecalculating` → `OnNavigationMeshUpdated`.

Console variables:

| Name | Default | Meaning |
|---|---|---|
| `goat_pathQueryThreads` | 2 | Worker threads, each with its own Detour query |
| `goat_pathQueryBudget` | 16 | Path queries submitted in one frame |
| `goat_navDefaultSpeed` | 3.0 | Speed used when a `move_to` node names none |
