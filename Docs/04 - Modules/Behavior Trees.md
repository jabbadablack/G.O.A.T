---
type: module
status: implemented
tags: [module, behavior-tree, backend, paradigm]
---

# Behaviour Trees (GOAT_BehaviorTree)

> **Status:** Implemented
> **Gem:** `GOAT_BehaviorTree`
> **Folder:** `Code/Source/Backends/BehaviorTree/`
> **Registers:** the `tree` decision backend

---

## What it is

The default paradigm, and the one most people reach for. A tree of composites and decorators with
verbs at the leaves; the walker finds the leftmost runnable leaf and runs it.

Put `tree` in an agent's **Brain** field. It is the default, so usually you do nothing.

It is a gem like any other. Deleting it leaves the core building and task networks running — that
is the test that the paradigm really is separable.

---

## The words

**Composites** hold children and decide the order:

| Word | Succeeds when |
| :--- | :--- |
| `selector` | any child succeeds — first one wins |
| `sequence` | every child succeeds, in order |
| `parallel` | per its policy, running children at once |
| `composite` | a custom composite, routed to a Lua `flow` |

**Decorators** wrap exactly one child:

| Word | Does |
| :--- | :--- |
| `invert` | flips success and failure |
| `force_success` | always succeeds |
| `loop` | repeats `count` times |
| `conditional_loop` | repeats while a condition holds |
| `time_limit` | fails after `seconds` |
| `cooldown` | refuses to re-run within `seconds` |
| `decorator` | a custom decorator, routed to a Lua `flow` |

**Others:**

| Word | Does |
| :--- | :--- |
| `service` | periodic work attached to a composite, on an `interval` |
| `subtree` | runs another tree by name, through a rebindable slot |

Plus the neutral words from `GOAT.lua` — `condition`, `compare`, `wait`, `raw`, `script`,
`delegate` — which every paradigm shares.

---

## A tree

```lua
return tree "CrowdWander" {
    selector {
        sequence {
            script "CrowdRoam",
            does_path_exist "move_target",
            move_to { key = "move_target", tolerance = 1.0, speed = 3.5 },
        },
        wait(1.0),
    },
}
```

Children run left to right. The `selector` tries the sequence, and falls back to waiting a second
if any part of it fails.

---

## Reactivity is the default

A `condition` is not only a test. **Declaring one declares a dependency.** The compiler records
the key it reads, and the agent is woken whenever that scope changes.

```lua
sequence {
    condition "crowd_rallying",
    move_to { key = "rally_point" },
}
```

If `crowd_rallying` goes false while `move_to` is running, the branch is dropped. You do not write
`abort` to get this.

`abort = "none"` is the explicit opt-out. `lower_priority` and `both` still mean what they always
did. An unrecognised mode **fails the compile** rather than quietly defaulting — that used to be a
silent typo, and it was the wrong kind of silent.

A condition is a leaf, so **what it guards is its parent's subtree**. That trips people up once.

---

## Subtrees and rebinding

`subtree "Patrol"` runs another tree by name, through a slot that can be pointed somewhere else at
runtime:

```lua
rebind_subtree "CombatStyle" { key = "chosen_style" }
```

A rebind recompiles every tree that used the slot. Agents already running an affected tree keep
the program they started on, so a rebind never rewrites a tree under an agent mid-action; they
pick the new one up next time they enter it.

---

## What's inside

| Piece | Does |
| :--- | :--- |
| `TreeCompiler` | flattens an [[AuthoredNode]] into a [[DecisionProgram]] |
| `TreeWalker` | finds the next runnable leaf |
| `DecisionCursor` | one agent's position in the tree — this is its brain state |
| `GuardEvaluator` | re-checks guards and drops the branch that broke |
| `ServiceTracker` | collects the services due this tick |
| `NodePredicate` | evaluates a `condition` or a `compare` |

The compiled program is a flat array in pre-order, so a node's index encodes its left-to-right
position — which is what makes "is this guard above the running leaf?" an integer comparison.

---

## Trees or task networks?

**A tree** when the *structure* is the design — priority order matters and you want to see at a
glance what beats what.

**A task network** when the *goal* is the design — several ways to achieve something and you want
a planner to pick. See [[Task Networks]].

Both can run in one level, on different agents. They talk through the blackboard.

---

## Related

- [[Task Networks]]
- [[Behavior DSL]]
- [[DecisionProgram]]
- [[IDecisionBackend]]
- [[Guard]]

---

*Last updated: 2026-08-27*
