---
type: asset
status: active
tags: [lua, authoring, api]
---

# GOAT.lua

> **Asset Type:** Lua Script  
> **Location:** `Assets/GOAT/Scripts/GOAT.lua`  
> **Executed:** Once at startup into the shared script context

---

## 📌 Purpose

`GOAT.lua` is the **authoring vocabulary** for G.O.A.T. It defines the global DSL functions (`tree`, `behavior`, `flow`, `backend`) and node constructors (`selector`, `sequence`, `wait`, `script`, etc.). Without this file, no trees, behaviors, backends, or flows can be authored.

It is executed once when the gem initializes, registering all global functions. The `tree` function compiles the node graph into a flat record list that C++ reads via `GOAT_EmitTree`.

---

## 🗝️ Internal State

The file initializes several global tables used to track declared content:

| Table | Description |
| :--- | :--- |
| `GOAT._behaviors` | Behaviours defined by `behavior`, keyed by name. |
| `GOAT._backends` | Backends defined by `backend`, keyed by name. |
| `GOAT._flow` | Control flows defined by `flow`, keyed by name. |
| `GOAT._trees` | Trees declared by `tree`, keyed by name (compiled records). |
| `GOAT._state` | Per-agent, per-behavior scratch tables. |

---

## 🗝️ Global Functions

### `tree(name, body)`
Declares a named behavior tree and compiles it. The body must be a single root node.

```lua
return tree "ExampleAgent" {
    selector {
        sequence {
            script "Patrol",
            wait(0.5),
        },
    },
}
```

Internally, `GOAT.Compile` flattens the node graph into a pre-order record list, stored in `GOAT._trees[name]`.

---

### `behavior(name, body)`
Defines a leaf behavior with optional `start`, `tick`, and `stop` phases.

```lua
behavior "Patrol" {
    start = function(me, ctx) me.stop = 0 end,
    tick = function(me, ctx)
        ctx:SetInt("patrol_stop", me.stop)
        return SUCCESS
    end,
}
```

---

### `flow(name, body)`
Defines custom control flow for composites/decorators.

```lua
flow "AllOf" {
    start = function(me, ctx, childCount) return 1 end,
    result = function(me, ctx, childIndex, childStatus)
        if childStatus == FAILURE then return nil, FAILURE end
        if childIndex < me.count then return childIndex + 1 end
        return nil, SUCCESS
    end,
}
```

---

### `backend(name, body)`
Defines a planning backend in Lua with a `plan` function.

```lua
backend "Errand" {
    plan = function(me, ctx, goal)
        return { { action = "wait", seconds = 2.0 } }
    end,
}
```

---

## 🗝️ Node Constructors

Node constructors are callable tables that build tree nodes. They accept a string argument (for the main property) or a table of children.

| Constructor | Default Property | Description |
| :--- | :--- | :--- |
| `selector` | None | Runs children until one succeeds. |
| `sequence` | None | Runs children until one fails. |
| `composite` | `behavior` | Runs a user-defined `flow` as a composite. |
| `invert` | None | Flips the child's result. |
| `force_success` | None | Forces the child to succeed. |
| `cooldown` | `seconds` | Blocks re-entry until cooldown expires. |
| `loop` | `count` | Repeats a fixed number of times. |
| `conditional_loop` | `key` | Repeats while a condition holds. |
| `time_limit` | `seconds` | Fails after duration. |
| `condition` | `key` | Checks a blackboard key. |
| `compare` | `key` | Compares two blackboard values. |
| `decorator` | `behavior` | Runs a user-defined `flow` as a decorator. |
| `wait` | `seconds` | Waits for a duration. |
| `script` | `behavior` | Runs a named behavior. |
| `raw` | `action` | Runs a registered verb directly. |
| `delegate` | `backend` | Delegates to a backend with a goal. |
| `subtree` | `tree` | Inlines another tree. |
| `service` | `behavior` | Attaches a periodic service. |

---

## 🗝️ Status Constants

```lua
RUNNING, SUCCESS, FAILURE = 0, 1, 2
```

---

## 🗝️ Context (`ctx`) Methods

The `ctx` object is passed to behaviors and backends.

| Method | Description |
| :--- | :--- |
| `ctx:GetSelf()` | Returns the entity ID. |
| `ctx:Has(name)` | True if variable is declared. |
| `ctx:GetBool(name)` / `ctx:SetBool(name, value)` | Boolean access. |
| `ctx:GetNumber(name)` / `ctx:SetNumber(name, value)` | Number access (Int or Float). |
| `ctx:GetVector3(name)` / `ctx:SetVector3(name, value)` | Vector3 access. |
| `ctx:GetEntity(name)` / `ctx:SetEntity(name, value)` | EntityId access. |
| `ctx:GetName(name)` / `ctx:SetName(name, value)` | Name string access. |

---

## 🗝️ Internal Helper Functions

| Function | Description |
| :--- | :--- |
| `GOAT.Compile(name, root)` | Flattens a node graph into a record list. |
| `GOAT._stateFor(agentKey, behaviorName)` | Returns the scratch table for a behavior/agent pair. |
| `GOAT_ForgetAgent(agentKey)` | Drops all scratch tables for an agent. |
| `GOAT_Dispatch(behaviorName, phase, agentKey, ctx, dt)` | C++ calls this to run a behavior phase. |
| `GOAT_EmitTree(treeName, builder)` | Hands a declared tree to a C++ builder. |
| `GOAT_TreeNames()` | Returns a list of declared tree names. |
| `GOAT_Plan(backendName, agentKey, ctx, goal, builder)` | Runs a Lua backend and pushes steps into a builder. |
| `GOAT_HasBackend(backendName)` | Returns true if the backend is defined. |
| `GOAT_EmitBackendNames(collector)` | Hands every backend name to a C++ collector. |
| `GOAT_FlowBegin/Advance/Filter` | Runs custom flow logic. |

---

## 🧪 Example Usage

```lua
-- Define a behavior
behavior "Patrol" {
    tick = function(me, ctx)
        ctx:SetInt("patrol_stop", me.stop)
        return SUCCESS
    end,
}

-- Define a tree
return tree "ExampleAgent" {
    selector {
        sequence {
            condition "target_seen" { abort = "lower_priority" },
            script "Alert",
            wait(1.0),
        },
        sequence {
            script "Patrol",
            wait(0.5),
        },
    },
}
```

---

## ⚠️ Constraints & Validation

- **Duplicate Names:** Behavior, flow, backend, and tree names must be unique.
- **Blackboard Keys:** Referencing an undeclared variable will fail at compile time.
- **Root Node:** A tree must have exactly one root node.

---

## 🔗 Related Assets & Notes

- [[Vocabulary]]
- [[Behavior DSL]]
- [[Flows]]
- [[Backends]]
- [[LuaDispatch]]
- [[LuaTreeBuilder]]

---

*Last updated: 2026-08-26*