---
type: asset
status: active
tags: [lua, authoring, api]
---

# Vocabulary

> **Asset Type:** Lua Script  
> **Location:** `Assets/GOAT/Scripts/GOAT.lua`  
> **Executed:** Once at startup into the shared script context

---

## 📌 Purpose

`GOAT.lua` is the **single source of truth** for the Lua authoring DSL. It is executed once when the gem initializes, registering all global functions and node constructors that designers use to create AI behavior. Without this file, no trees, behaviors, backends, or flows can be authored.

---

## 🗝️ Global Functions

These are the top-level functions that define the building blocks of a GOAT agent.

| Function | Description |
| :--- | :--- |
| `tree(name, body)` | Declares a named behavior tree and compiles it into a flat node list for C++ execution. |
| `behavior(name, body)` | Defines a leaf behavior with optional `start`, `tick`, and `stop` functions. |
| `flow(name, body)` | Defines custom composite/decorator control flow logic. |
| `backend(name, body)` | Defines a planning backend in Lua, with a `plan` function that returns steps. |

### `tree`

Declares a tree. The `body` must be a single root node.

```lua
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

### `behavior`

Defines a leaf behavior. `me` is a per-agent scratch table for this behavior; `ctx` is the blackboard context.

```lua
behavior "Patrol" {
    start = function(me, ctx)
        me.stop = 0
    end,
    tick = function(me, ctx)
        me.stop = me.stop + 1
        ctx:SetInt("patrol_stop", me.stop)
        return SUCCESS
    end,
}
```

### `flow`

Defines custom control flow for composites/decorators.

```lua
flow "AllOf" {
    start = function(me, ctx, childCount)
        me.count = childCount
        return 1
    end,
    result = function(me, ctx, childIndex, childStatus)
        if childStatus == FAILURE then return nil, FAILURE end
        if childIndex < me.count then return childIndex + 1 end
        return nil, SUCCESS
    end,
}
```

### `backend`

Defines a Lua planning backend. The `plan` function receives `me`, `ctx`, and `goal`, and returns a list of steps.

```lua
backend "Errand" {
    plan = function(me, ctx, goal)
        if goal == "Rest" then
            return { { action = "wait", seconds = 2.0 } }
        end
        return {
            { action = "script", behavior = "Announce" },
            { action = "wait", seconds = 0.5 },
        }
    end,
}
```

---

## 🗝️ Node Constructors

Node constructors are callable tables that build tree nodes. They accept a string argument (for the main property), a table of children, or both.

### Composites

| Constructor | Description |
| :--- | :--- |
| `selector` | Runs children in order until one succeeds. |
| `sequence` | Runs children in order until one fails. |
| `composite` | Runs a user-defined `flow` as a composite. |

### Decorators

| Constructor | Description |
| :--- | :--- |
| `invert` | Inverts the child's result (Success → Failure, Failure → Success). |
| `force_success` | Forces the child's result to Success. |
| `cooldown` | Prevents the child from running until the cooldown expires. |
| `loop` | Repeats the child a fixed number of times. |
| `conditional_loop` | Repeats the child while a condition holds. |
| `time_limit` | Fails the child if it takes longer than the specified time. |
| `condition` | Checks a blackboard key. |
| `compare` | Compares a blackboard key to a value. |
| `decorator` | Runs a user-defined `flow` as a decorator. |

### Leaves

| Constructor | Description |
| :--- | :--- |
| `wait` | Waits for a specified number of seconds. |
| `script` | Runs a named `behavior`. |
| `raw` | Runs a registered action verb by name (e.g., `MoveTo`, `Wait`). |
| `delegate` | Delegates to a named backend with a goal. |
| `subtree` | Inlines another named tree. |

### Services

| Constructor | Description |
| :--- | :--- |
| `service` | Attaches a periodic behavior to a composite, running at a fixed interval. |

---

## 🗝️ Status Constants

These are the return values that behavior `tick` functions use to signal the tree walker.

| Constant | Value | Description |
| :--- | :--- | :--- |
| `RUNNING` | 0 | The behavior is still in progress. |
| `SUCCESS` | 1 | The behavior completed successfully. |
| `FAILURE` | 2 | The behavior failed. |

---

## 🗝️ Context (`ctx`) Methods

The `ctx` object is passed to every behavior `tick` function. It provides type-safe access to the blackboard.

| Method | Description |
| :--- | :--- |
| `ctx:SetBool(name, value)` | Writes a boolean to the blackboard. |
| `ctx:GetBool(name)` | Reads a boolean from the blackboard. |
| `ctx:SetInt(name, value)` | Writes an integer to the blackboard. |
| `ctx:GetInt(name)` | Reads an integer from the blackboard. |
| `ctx:SetFloat(name, value)` | Writes a float to the blackboard. |
| `ctx:GetFloat(name)` | Reads a float from the blackboard. |
| `ctx:SetVector3(name, value)` | Writes a `Vector3` to the blackboard. |
| `ctx:GetVector3(name)` | Reads a `Vector3` from the blackboard. |

---

## 🧪 Example Usage

```lua
-- Define a behavior that writes to the blackboard
behavior "Sense" {
    tick = function(me, ctx)
        ctx:SetBool("target_seen", ctx:GetInt("patrol_stop") % 4 == 0)
    end,
}

-- Define a tree that uses it
return tree "ExampleAgent" {
    selector {
        service "Sense" { interval = 0.25 },
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
- **Required Properties:** Some nodes require certain properties (e.g., `wait` requires `seconds`).
- **Blackboard Keys:** Referencing an undeclared variable will fail at compile time.
- **Root Node:** A tree must have exactly one root node.

---

## 🔗 Related Assets & Notes

- [[Behavior DSL]]
- [[Flows]]
- [[Backends]]
- [[LuaDispatch]]

---

*Last updated: 2026-08-26*