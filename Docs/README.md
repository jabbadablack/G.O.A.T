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

## Directors

A **GOAT Director** watches the other agents and reshapes what they are doing. It is an agent
itself: it registers with the agent system and runs an ordinary behaviour tree, so guards,
services, `parallel`, plans and the console all apply to it unchanged. Its leaves act on the agents
it governs rather than on itself.

**Which agents it governs is its reach**, set on the component. The filters that are set narrow it,
so a director with none governs the whole level:

| Field | Governs only |
|---|---|
| Squad | agents in that squad |
| Tree | agents currently running that tree |
| Radius | agents that close |
| Filter | whatever a module's reach filter says, such as `path_distance` from the navigation gem |

Several directors may govern the same agent. **Priority settles it** — higher outranks lower, and
the loser is dropped and traced. An agent switching its own tree carries priority zero, so any
director outranks it.

### The five words

```lua
order_tree      "Assault"      { key = "director_target", limit = 4 }
order_interrupt "Flee"         { limit = 2 }
order_band      (3)
order_value     "alert_level"  { value = 2 }
rebind_subtree  "combat_style" { key = "director_style" }
```

`key` names an `EntityId` variable: set and naming an agent in reach, the verb commands that one;
unset, it commands them all. `limit` caps how many, which is what lets a director escalate rather
than flip a whole population in one tick.

A verb **succeeds when it changed at least one agent and fails when it changed none**, so a
`selector` branches on being refused with no new machinery.

Four variables come declared with the gem, so a director tree compiles without a `.bbx` of its own:
`director_reach`, `director_changed` and `director_refused` are what the last verb did, and
`director_target` is where a director's sensing names the one agent a verb should narrow to.

### Publish, do not command

`order_value` is the lever to reach for. It writes a variable and each agent's own guards decide
whether and how to react — an order the agent arbitrates, which is how F.E.A.R.'s squad orders
worked. `order_tree` is the blunt instrument: it stops whatever the agent was doing.

**Who a write reaches is decided by the variable's declared scope**, which is why the verb needs no
parameter saying so: a `Global` variable is written once, a `Squad` one once per squad in reach, an
`Agent` one once per agent. It cannot narrow to a single agent, deliberately — pushing exact truth
into one agent's private blackboard is what Alien: Isolation's designers found destroys the
behaviour you wanted. The director channel is meant to be lossy.

### Why it cannot thrash

Switching a tree aborts the running action and resets the cursor, so a director doing it every tick
would leave its agents permanently restarting. Two rules in C++ make that unreachable rather than
merely documented:

- **Idempotent.** Ordering a tree an agent is already on, or a band it is already in, does nothing
  — and a no-op neither spends a cooldown nor starts one.
- **Cooled down.** Each director has its own cooldown per agent per verb. Per director, so one
  director's order can never silence another's.

### Sensing

```lua
for i = 1, ctx:CountInReach() do
    local e = ctx:GetInReach(i)
    if ctx:GetNumberOf(e, "nav_remaining") < 1.0 then
        ctx:SetEntity("director_target", e)
    end
end
```

Plus `CountRunning(tree)`, `GetTreeOf`, `GetSquadOf`, `GetBandOf`, `GetBoolOf`. These hand back
**entities, never agent handles**: an entity stops naming anyone once its agent is gone, where a
remembered position would quietly name a different agent after the roster compacted. Writing to
another agent's blackboard is deliberately not offered — `order_value` is the only write channel.

### Rebinding a subtree

`subtree { tag = "combat_style" }` in a tree is a slot a director fills. `rebind_subtree` points it
at a different tree and recompiles every tree that used it. **A rebind changes what agents will run
next, not what they are running now** — an agent mid-action finishes on the program it started, and
takes the new one the next time it enters that tree. A director that wants it immediately rebinds
and then orders the same tree name.

## Looking at what is happening

| Command | Prints |
|---|---|
| `GOATSystemComponent.ListAgents` | Every agent, its tree, and what it is doing |
| `GOATSystemComponent.DumpAgent <entityId>` | One agent in detail |
| `GOATSystemComponent.ListTrees` / `ListPlans` | What is compiled and declared |
| `GOATSystemComponent.DumpPlan <name>` | One plan's options and steps |
| `GOATSystemComponent.ListBackends` / `ListActions` / `ListNodes` | What is installed |
| `GOATSystemComponent.ListDirectors` | Every director, its reach and how many it governs |
| `GOATSystemComponent.DumpDirector <entityId>` | Exactly the agents one director governs |
| `GOATSystemComponent.ListReachFilters` / `ListSquads` | What modules contributed, and who is grouped |
| `GOATSystemComponent.RebindSubtreeCommand <slot> <tree>` | Point a subtree slot somewhere else |
| `GOATSystemComponent.ValidatePlans` | Re-check every plan now |

Log channels, each toggled with `LoggerSystemComponent.EnableLog <tag>`:
`GoatAgent` (plan boundaries and aborts) · `GoatPlan` (which option a plan chose) ·
`GoatNav` (path queries) · `GoatSmartObject` (claims) · `GoatDirector` (refused orders, rebinds).
