---
type: asset
status: active
tags: [lua, authoring, api]
---

# Vocabulary

> **Asset Type:** Lua Script
> **Core file:** `Assets/GOAT/Scripts/GOAT.lua`
> **Gem files:** each backend gem ships its own, run straight after
> **Executed:** Once at startup into the shared script context

---

## 📌 Purpose

The vocabulary is what you type when you author AI. It is loaded once at startup, and it comes
from more than one file.

`GOAT.lua` holds only what **every paradigm shares** — `condition`, `compare`, `wait`, `raw`,
`script`, `delegate`, `embed`, plus the machinery (`behavior`, `flow`, `plan`, `backend`, `GOAT.Compile`).

Each backend gem ships its own vocabulary file, run straight after:

| File | Brings |
| :--- | :--- |
| `Assets/GOAT/Scripts/GOAT.lua` | the shared words and the machinery |
| `GOAT_BehaviorTree/Scripts/BehaviorTree.lua` | `tree`, `service`, `subtree` |
| `GOAT_Htn/Scripts/Htn.lua` | `domain`, `task`, `method`, `primitive`, `subtask`, `effect` |

Module gems add verbs the same way — `move_to` comes from the navigation gem.

**`tree` and `selector` are not core words.** If they ever appear in `GOAT.lua` again, the
paradigm split has failed. Turn off the behaviour tree gem and those words are simply gone, while
`condition` and `script` keep working.

Most words declare themselves. `GOAT_DeclareNode(typeName, mainProperty)` creates a global for any
registered node type, so a gem writes one line per word instead of a constructor.

---

## 🗝️ Global Functions

These are the top-level functions that define the building blocks of a GOAT agent.

| Function | Description |
| :--- | :--- |
| `behavior(name, body)` | Defines a leaf behavior with optional `start`, `tick`, and `stop` functions. |
| `flow(name, body)` | Defines custom composite/decorator control flow logic. |
| `backend(name, body)` | Defines a `delegate` planner in Lua, with a `plan` function that returns steps. |
| `plan(name, body)` | Declares the same thing declaratively, as a list of `option`s. Baked at load. |
| `tree(name, body)` | Declares a behaviour tree. **From the GOAT_BehaviorTree gem.** |
| `domain(name, body)` | Declares a task network. **From the GOAT_Htn gem.** |

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

Node constructors are callable tables that build nodes. They take a string (which fills the main
property) or a table of children.

They come from different places, and it matters: turn a gem off and its words go with it.

### Shared — from `GOAT.lua`, available to every paradigm

| Constructor | Main property | Description |
| :--- | :--- | :--- |
| `condition` | `key` | Checks a blackboard value, and aborts the branch it sits in when it changes. |
| `compare` | `key` | Compares two blackboard values, aborting on either changing. |
| `wait` | `seconds` | Waits for a number of seconds. |
| `script` | `behavior` | Runs a named `behavior`. |
| `raw` | `action` | Runs any registered verb directly, including one a module contributed. |
| `delegate` | `backend` | Hands an intent to a named planner, or to a whole paradigm, for one plan. |
| `embed` | `goal` | Runs another program, in whatever paradigm owns it, until it is done. |

> **`condition` is a leaf, not a decorator.** It evaluates and reports; what it guards is the
> branch it *sits in*, not a child of its own. This catches everyone once.
>
> It is also a **dependency declaration**. Writing one makes the agent react when that value
> changes — you do not need `abort` for that. `abort = "none"` opts out.

### Behaviour tree — from the GOAT_BehaviorTree gem

| Constructor | Main property | Description |
| :--- | :--- | :--- |
| `selector` | — | Runs children in order until one succeeds. |
| `sequence` | — | Runs children in order until one fails. |
| `parallel` | — | Runs children at once, per its policy. |
| `composite` | `behavior` | Runs a user-defined `flow` as a composite. |
| `invert` | — | Inverts the child's result. |
| `force_success` | — | Forces the child's result to success. |
| `cooldown` | `seconds` | Prevents re-entry until the cooldown expires. |
| `loop` | `count` | Repeats the child a fixed number of times. |
| `conditional_loop` | `key` | Repeats while a condition holds. |
| `time_limit` | `seconds` | Fails the child if it takes too long. |
| `decorator` | `behavior` | Runs a user-defined `flow` as a decorator. |
| `subtree` | `tree` | Runs another named tree through a rebindable slot. |

### Task network — from the GOAT_Htn gem

| Constructor | Main property | Description |
| :--- | :--- | :--- |
| `task` | `name` | Something to achieve, offering methods. |
| `method` | — | One way to achieve a task. |
| `primitive` | `name` | A leaf that runs a verb. |
| `subtask` | `task` | Names another task or primitive from inside a method. |
| `effect` | `key` | What a primitive is assumed to change while planning. |

### Verbs — from module gems

| Constructor | Main property | From |
| :--- | :--- | :--- |
| `move_to`, `is_at_location`, `does_path_exist` | `key` | Navigation |
| `claim_smart_object`, `use_smart_object` | `use` | Smart Objects |
| `play_motion`, `animate` | — | Animation |
| `order_tree`, `order_interrupt`, `order_band`, `order_value`, `rebind_subtree` | varies | the core, for directors |

### Services — from the GOAT_BehaviorTree gem

| Constructor | Main property | Description |
| :--- | :--- | :--- |
| `service` | `behavior` | Attaches periodic work to a composite, on an `interval`. |

A service attaches to a composite rather than sitting in its child list, which is why it is the
one word the gem writes out by hand instead of letting `GOAT_DeclareNode` generate it.

Reach for a service when you genuinely need work on a timer. Do **not** use one to poll a
blackboard variable — a `condition` already reacts the moment that variable changes, with no
interval and no cost while nothing happens.

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

- **Duplicate names.** Behaviour, flow, backend, tree and domain names must each be unique, and a
  `plan` cannot take a name a backend already has.
- **Required properties.** Some words demand one — `wait` needs `seconds`, `task` needs a `name`.
- **Blackboard keys.** Referencing an undeclared variable fails at compile time.
- **One root.** A tree has exactly one root node. A domain starts at its first task unless `root`
  names another.
- **Unknown `abort` modes fail the compile.** They used to fall through to `none` silently, which
  turned a typo into a tree that quietly stopped reacting.
- **A word you did not enable does not exist.** `selector` with the behaviour tree gem turned off
  is an undefined global, not a compile error with a helpful message.

---

## 🔗 Related Assets & Notes

- [[Behavior DSL]]
- [[Flows]]
- [[Backends]]
- [[GOAT.lua]]
- [[AgentScriptContext]]

---

*Last updated: 2026-08-26*