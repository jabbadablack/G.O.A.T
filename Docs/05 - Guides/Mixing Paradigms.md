---
type: guide
status: active
tags: [guide, backend, composition]
---

# Mixing Paradigms

> **You should have read:** [[Backend Abstraction Theory]], [[Behavior Trees]], [[Task Networks]]
> **Core files:** `Code/Source/Core/Actions/EmbedAction.cpp`,
> `Code/Source/Core/Application/DecisionBackendAdapter.cpp`,
> `Code/Source/Core/Application/NestedRun.cpp`

---

## The idea

A paradigm no longer has to own a whole agent. A node of one program can hand its work to a
program written in another, so a behaviour tree runs inside an HTN task, an HTN goal runs inside
a behaviour tree, and whatever ships next runs inside either. A [[Utility AI]] choice embedding
one of each is the shape this was built for: scoring picks the course of action, and whichever
paradigm expresses it best carries it out.

Programs are named, never inlined. An `embed` names a program; which paradigm owns it is
answered by the word its root is written as, so `domain "Sweep"` is an HTN program and
`tree "Sweep"` is a tree, and nothing has to say so twice.

---

## Three words that look alike

This is the table to read before reaching for any of them.

| | `subtree` | `delegate` | `embed` |
|---|---|---|---|
| Owned by | the tree gem | the core | the core |
| When it resolves | compile time | every time the leaf runs | every time the leaf runs |
| Crosses paradigms | no | yes | yes |
| Runs for | no time of its own | one plan | as long as it takes |
| Its own guards | no, it shares the host's | n/a | yes |
| Its own brain state | no, it shares the host's cursor | borrowed for one call | borrowed until it ends |

**`subtree "Guard"`** is flattened into the host when it compiles. There is no subtree at run
time — the nodes are simply there, sharing one cursor and one abort scope. Use it to factor a
tree, and to get rebindable slots ([[IAgentSystem]]`::RebindSubtree`).

**`delegate "htn" { goal = "SecurePerimeter" }`** asks something once and takes the one plan it
gives back. The leaf's result is that plan's result. It reaches an [[IBackend]] planner if one
is registered under that name, and otherwise a whole paradigm answering the same question. This
is the cheap one: nothing is kept between calls.

**`embed "ClearRoom"`** runs another program until it is done. The leaf reads Running the whole
time, then whatever the nested program ended as.

> **`delegate` is a behaviour tree leaf, and only that.** The core reserves the name — 
> `IAgentSystem::RegisterAction` asserts against a verb called `delegate` — because a plan step
> naming it would let a plan re-enter the tree that asked for it. So a paradigm whose bodies are
> plans rather than trees, which is both [[Task Networks]] and [[Utility AI]], reaches another
> paradigm with `embed`. Its compiler should say so rather than reporting a missing verb.

---

## Writing it

### A behaviour tree inside an HTN task

```lua
tree "ClearRoom" {
    sequence {
        move_to "room_door",
        script "SweepCorners",
    },
}

domain "SecurePerimeter" {
    root = "Secure",

    task "Secure" {
        method { condition "breach_found", subtask "Clear" },
        method { subtask "Patrol" },
    },

    primitive "Clear" {
        embed "ClearRoom",
        effect "room_clear",
    },

    primitive "Patrol" { wait(2.0) },
}
```

### An HTN goal inside a behaviour tree

An "HTN goal" is a `domain` declared with that goal as its root. Two domains over the same tasks
with different roots are two goals.

```lua
tree "Guard" {
    selector {
        sequence {
            condition "alarm" { abort = "self" },
            delegate "htn" { goal = "SecurePerimeter" },
        },
        wait(1.0),
    },
}
```

---

## What interruption does

Both directions work, and they are not the same thing.

**Outward — the host tears the nested one down.** A guard closing on the host, an
`order_interrupt`, an agent going away: all of them end the step the nested program is running
in, and ending that step is what releases its plan, tells its backend it is finished, and gives
back the part of the brain block it borrowed. The host's leaf then fails, like any leaf.

**Inward — the nested one interrupts itself.** It gets its own `Advance`, so its own guards fire
and it abandons and replans behind the leaf. The host never hears about it; from outside, the
step is still running.

The host is woken for what the nested program watches. Compiling folds the nested program's
watched scopes into the host's, so an agent asleep inside a nested run still wakes when a
variable that run guards on changes. What it does **not** fold is `m_wantsTick` — a program with
services is only ticked hard while it is actually running, not for the whole life of any agent
that might one day reach it.

---

## What it costs

One nesting level roughly doubles the cost of a tick for the agents actually inside one:
`BM_TickNested` against `BM_TickBand` is about 2.6x, or 34 ns against 13 ns per agent. Agents
that nest nothing are not affected — `AgentRuntime::Tick` is not on this path at all.

Brain state is sized when a program compiles, as the most any one chain under it needs rather
than the sum, so two `embed` leaves in different branches cost one of them and not two. An agent
that nests nothing pays for its own backend and nothing else.

---

## Limits worth knowing

- **Four deep.** `MaxNestDepth`. A program that hands work back to itself, directly or through
  another, fails to compile rather than looping.
- **`time_limit { embed "X" }` does not cut a nested run short.** It is checked when its child
  finishes, which is true of every leaf, not just this one. For a bound that fires mid-run, use
  a `condition` with `abort = "self"` beside it.
- **A nested program that never says it is finished never finishes.** A backend reports that
  through `Decision::m_result`; one that never sets it runs until the host stops it.
- **A nested program that never produced a plan fails the step it sits in**, whatever it says
  about how it ended.
- **Lua behaviour scratch is keyed by agent and behaviour name**, so a `script "Sense"` in the
  host and one in the nested program share a table. That has always been true across `subtree`;
  it now reaches across paradigms too.
- **An HTN `primitive` cannot host a `delegate`**, because it resolves its operator as a verb and
  `delegate` is not one. `embed` is the word for that direction.

---

## Adding it to a new backend

Almost nothing. A word that hands work to a named program sets `m_nestsProgram` on its
[[NodeTypeDescriptor]], and the compiler records the reference on `AgentProgram::m_nested`. The
core does the rest: it compiles what was named, folds in what it needs, and refuses a cycle.

To be embeddable in the other direction, a backend only has to say when it is done — set
`Decision::m_result` when it produces no plan because it has finished rather than because it is
waiting. Not every paradigm can: [[Utility AI]] never reports finished, because a choice argues
from numbers that move and asking again later can answer differently, so an embedded utility
program is ended by its host's guards the way a `wait` is. That is a real answer, not an
omission — but it is the host author who needs to know it.

See [[Writing Custom Backends]] for the rest of a paradigm gem.
