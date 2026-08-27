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

It defines the syntax that gets compiled into a `DecisionProgram` for the C++ runtime.

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
    selector { ... }
}
```

### `behavior`
Defines a leaf behavior. `me` is a per-agent scratch table for this behavior; `ctx` is the blackboard context.
```lua
behavior "Patrol" {
    start = function(me, ctx) me.stop = 0 end,
    tick = function(me, ctx)
        ctx:SetInt("patrol_stop", me.stop)
        return SUCCESS
    end,
}
```

### `flow`
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

### `backend`
Defines a Lua planning backend.
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

### Composites
| Constructor | Default Property | Description |
| :--- | :--- | :--- |
| `selector` | None | Runs children in order until one succeeds. |
| `sequence` | None | Runs children in order until one fails. |
| `composite` | `behavior` | Runs a user-defined `flow` as a composite. |

### Decorators
| Constructor | Default Property | Description |
| :--- | :--- | :--- |
| `invert` | None | Inverts the child's result. |
| `force_success` | None | Forces the child's result to Success. |
| `cooldown` | `seconds` | Prevents re-entry until cooldown expires. |
| `loop` | `count` | Repeats the child a fixed number of times. |
| `conditional_loop` | `key` | Repeats while a condition holds. |
| `time_limit` | `seconds` | Fails the child if it takes too long. |
| `condition` | `key` | Checks a blackboard key. |
| `compare` | `key` | Compares a blackboard key to another value. |
| `decorator` | `behavior` | Runs a user-defined `flow` as a decorator. |

### Leaves
| Constructor | Default Property | Description |
| :--- | :--- | :--- |
| `wait` | `seconds` | Waits for a specified number of seconds. |
| `script` | `behavior` | Runs a named `behavior`. |
| `raw` | `action` | Runs a registered action verb by name. |
| `delegate` | `backend` | Delegates to a named backend with a goal. |
| `subtree` | `tree` | Inlines another named tree. |

### Services
| Constructor | Default Property | Description |
| :--- | :--- | :--- |
| `service` | `behavior` | Attaches a periodic behavior to a composite, running on an interval. |

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

The `ctx` object is passed to every behavior `tick` function. It provides type-safe access to the blackboard. (Note: `GetInt`/`SetInt` and `GetFloat`/`SetFloat` are aliases for `GetNumber`/`SetNumber`).

| Method | Description |
| :--- | :--- |
| `ctx:GetSelf()` | Returns the entity ID of the agent. |
| `ctx:Has(name)` | Returns true if the variable is declared. |
| `ctx:GetBool(name)` | Reads a boolean from the blackboard. |
| `ctx:SetBool(name, value)` | Writes a boolean to the blackboard. |
| `ctx:GetNumber(name)` | Reads an int or float slot as a number. |
| `ctx:SetNumber(name, value)` | Writes to an int or float slot. |
| `ctx:GetVector3(name)` | Reads a `Vector3` from the blackboard. |
| `ctx:SetVector3(name, value)` | Writes a `Vector3` to the blackboard. |
| `ctx:GetEntity(name)` | Reads an `EntityId` from the blackboard. |
| `ctx:SetEntity(name, value)` | Writes an `EntityId` to the blackboard. |
| `ctx:GetName(name)` | Reads a `Name` string from the blackboard. |
| `ctx:SetName(name, value)` | Writes a `Name` string to the blackboard. |

---

## 🗝️ Backend Step Properties

The properties a Lua backend can return in its step table (via `LuaPlanBuilder`):

| Property | Type | Description |
| :--- | :--- | :--- |
| `action` | String | (Required) The verb to run (e.g., `wait`, `script`). |
| `behavior` | String | Used with `action = "script"`. |
| `tag` | String | A tag to pass to the action (e.g., animation clip name). |
| `seconds` | Number | Duration for `wait`. |
| `tolerance` | Number | Tolerance for movement. |
| `key` | String | Blackboard variable name for the target. |

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
- [[GOAT.lua]]
- [[AgentScriptContext]]

---

*Last updated: 2026-08-26*