---
type: module
status: implemented
tags: [module, htn, backend, paradigm]
---

# Task Networks (GOAT_Htn)

> **Status:** Implemented
> **Gem:** `GOAT_Htn`
> **Folder:** `Code/Source/Backends/Htn/`
> **Registers:** the `htn` decision backend
> **Log tag:** `GoatHtn` — turn it on with `EnableLog GoatHtn`

---

## What it is

A hierarchical task network. You describe tasks, each task offers methods, and each method is a
list of subtasks with a condition on the front. Planning walks down from a root task picking the
first method whose condition holds, until what is left is a flat list of primitives to run.

It is total-order forward decomposition — Humphreys' formulation, the one Horizon Zero Dawn used.

Put `htn` in an agent's **Brain** field to use it. It is an ordinary gem: delete it and the rest
of GOAT still builds.

---

## The words

| Word | Main property | What it is |
| :--- | :--- | :--- |
| `domain` | name | the whole network |
| `task` | `name` | something to achieve, offering methods |
| `method` | — | one way to achieve a task |
| `primitive` | `name` | a leaf that actually runs a verb |
| `subtask` | `task` | names another task or primitive from inside a method |
| `effect` | `key` | what a primitive is assumed to change while planning |

`condition`, `compare`, `wait`, `script` and the module verbs are shared with every other
paradigm, so they work here unchanged.

---

## A domain

```lua
return domain "CrowdMarshal" {
    root = "Marshal",
    task "Marshal" {
        method {
            condition "crowd_rallying",
            subtask "Order",
            subtask "Report",
            subtask "Sense",
        },
        method {
            subtask "Sense",
            subtask "Rest",
        },
    },
    primitive "Order" { order_interrupt "CrowdRally" { limit = 6 } },
    primitive "Report" { script "ReportMuster" },
    primitive "Sense" { script "CallMuster" },
    primitive "Rest" { wait(1.0) },
}
```

Methods are tried in order. The first whose condition holds wins, and a method with no condition
is the fallback — so put it last.

Planning starts at the first task unless `root` names another.

---

## Effects, and why they exist

A method's condition is checked **at decomposition time**, before any subtask has run. So if step
one is meant to make step two's condition true, the planner has to be told:

```lua
primitive "OpenDoor" {
    effect { key = "door_open", value = true },
    script "Open",
}
```

`effect` says "assume this changes while planning". The planner applies it to a working copy of
the world as it decomposes, so a later method can condition on it.

Effects are a **planning fiction**. They do not write the blackboard. The primitive's actual verb
has to do that for real, or the plan will be built on something that never happened.

---

## When a plan gets dropped

`Advance` re-validates the plan whenever a watched scope changes, and abandons it if a remaining
step no longer holds.

It walks **only the primitives that have not run yet**, from the current step, applying each one's
effects as it goes.

> **It deliberately does not re-check the method preconditions.** Those chose the plan. Re-checking
> them is the classic replan-on-own-effects bug: a plan whose own steps write the variable its
> method conditioned on will abandon itself every single tick, forever. The `CrowdMarshal` above
> reproduces it exactly — its `Sense` step writes `crowd_rallying`, which its first method
> conditions on.

`m_watchedScopes` is narrowed to **condition** keys only. A domain should not wake on a scope it
merely writes.

---

## Limits

| Constant | Value | Why |
| :--- | :--- | :--- |
| `MaxDecomposeDepth` | 16 | how deep tasks may nest |
| `MaxPlanTasks` | 32 | steps in one plan |

`MaxPlanTasks` is 32 so the validation record fits the 72-byte brain state (32 × `u16` plus a
count is 66). Thirty-two primitive steps in one plan is already generous.

---

## Diagnostics

`EnableLog GoatHtn` in the console. It fires for **every** agent on the `htn` backend, not only
directors:

```
GOAT: agent 12 domain 'CrowdMarshal' planned 3 step(s): Order -> Report -> Sense [chose Marshal#0]
GOAT: agent 12 domain 'CrowdMarshal' found no plan from task 'Marshal'
GOAT: agent 12 domain 'CrowdMarshal' dropped its plan at step 1 of 3 ('Report'): crowd_rallying is not true
```

`[chose Marshal#0]` is the method traversal record — task `Marshal` was carried out by its method
0. That is the diagnostic a task network needs and a tree does not: the plan tells you what the
agent will do, the choices tell you why it is not doing something else.

Costs nothing with the tag off — the strings are built inside the `AZLOG`, so with the tag
disabled the whole thing is one hash compare.

---

## Trees or task networks?

Neither is better. They answer different questions.

**A tree** is good when the *structure* is the design — priority order matters, and you want to
see at a glance what beats what. Reactivity is natural: a guard higher in the tree wins.

**A task network** is good when the *goal* is the design — several ways to achieve something, and
you want the planner to pick. It handles "do A then B, but only if A is possible" without you
hand-wiring the fallbacks.

You can use both in one level. They talk through the blackboard and neither knows the other
exists.

---

## Related

- [[Behavior Trees]]
- [[IDecisionBackend]]
- [[Backend Abstraction Theory]]
- [[Director AI]]

---

*Last updated: 2026-08-27*
