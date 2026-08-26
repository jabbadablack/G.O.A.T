# G.O.A.T

A pluggable NPC AI framework for O3DE. C++ supplies the infrastructure; behaviour is written in
Lua, and every stage talks to every other through blackboard variables authored as `.bbx` assets.

```
   Lua BT          Backend            FSM              Agent
  ─────────  ──►  ─────────  ──►  ───────────  ──►  ─────────
  a tree          turns one        runs one          the entity
  emits an        Intent into      action at         that acts
  Intent          an ActionPlan    a time
      │               │                │                 │
      └───────────────┴────────────────┴─────────────────┘
                     BLACKBOARD  (global / agent / squad)
```

## The gems

| Gem | Adds | Needs |
|---|---|---|
| `GOAT` | The core: trees, blackboards, the FSM, the Lua vocabulary | LmbrCentral |
| `GOAT_Navigation` | `move_to`, `is_at_location`, `does_path_exist` | RecastNavigation |
| `GOAT_SmartObject` | `claim_smart_object`, `use_smart_object` | — |
| `GOAT_Animation` | `animate`, `play_motion` | EMotionFX |

The core is genre neutral: it names none of the modules and depends on none of them. A project
that never needs a navigation mesh does not enable that gem, and `move_to` does not exist.

## Getting an agent running

1. Author a `.bbx` in the Asset Editor declaring the variables your behaviour reads and writes.
   Names are global across every `.bbx`, so a duplicate is an error rather than an override.
2. Write a `.lua` declaring a tree, and the behaviours its leaves run.
3. Add **GOAT Agent** to an entity, point it at both, and name the trees it may run.

```lua
behavior "Patrol" {
    tick = function(me, ctx)
        ctx:SetInt("patrol_stop", (ctx:GetInt("patrol_stop") + 1) % 4)
        return SUCCESS
    end,
}

return tree "Guard" {
    selector { service "Sense" { interval = 0.25 },
        sequence {
            condition "target_seen" { abort = "lower_priority" },
            script "Alert",
        },
        script "Patrol",
    },
}
```

## The vocabulary

| Kind | Words |
|---|---|
| Composites | `selector` `sequence` `parallel` `composite` |
| Decorators | `invert` `force_success` `cooldown` `loop` `conditional_loop` `time_limit` `decorator` |
| Leaves | `condition` `compare` `wait` `script` `raw` `delegate` `subtree` |
| Attached | `service` |
| Declaring | `behavior` `flow` `backend` `plan` `option` `tree` |

Modules add their own: enabling `GOAT_Navigation` adds `move_to`, and disabling it takes the word
away again. A tree naming a word no module provides fails to compile with a message saying so.

## Reacting without polling

A `condition` with an abort mode is an **observer**: it wakes only when a blackboard variable it
reads actually changes. An agent whose blackboard is quiet evaluates no conditions at all. The
four abort modes are Unreal's — `none`, `self`, `lower_priority`, `both`.

A `service` is the other half: it turns polling into blackboard writes at a fixed interval, and
only while execution is inside the subtree it is attached to.

## Plans

A `delegate` leaf hands a goal to a backend, which answers with a *sequence* of steps the agent
commits to. See `Code/Source/Backends/README.md` for the `plan` vocabulary and the `bt` backend.

Plans have no length limit. An authored plan's steps are baked once when the vocabulary loads and
shared by every agent running it, so a five hundred step plan costs an agent the same as a one step
plan and reaching a plan boundary copies nothing.

## Switching trees

An agent names several trees and moves between them. All of them compile when the entity
activates, so a name it only switches to much later still fails immediately.

```lua
ctx:SetTree("Combat")     -- replace, forgetting what was interrupted
ctx:PushTree("Flee")      -- interrupt, remembering
ctx:PopTree()             -- go back
```

A switch ends the running action first, so a verb gives back what it holds — a pooled path slot, a
smart object claim. A switch asked for from inside a behaviour lands on the agent's next tick,
because the current one is holding references to the tree it would replace.

## Looking at what is happening

| Command | Prints |
|---|---|
| `GOATSystemComponent.ListAgents` | Every agent, its tree, and what it is doing |
| `GOATSystemComponent.DumpAgent <entityId>` | One agent in detail |
| `GOATSystemComponent.ListTrees` / `ListPlans` | What is compiled and declared |
| `GOATSystemComponent.DumpPlan <name>` | One plan's options and steps |
| `GOATSystemComponent.ListBackends` / `ListActions` / `ListNodes` | What is installed |
| `GOATSystemComponent.ValidatePlans` | Re-check every plan now |

Log channels, each toggled with `LoggerSystemComponent.EnableLog <tag>`:
`GoatAgent` (plan boundaries and aborts) · `GoatPlan` (which option a plan chose) ·
`GoatNav` (path queries) · `GoatSmartObject` (claims).
