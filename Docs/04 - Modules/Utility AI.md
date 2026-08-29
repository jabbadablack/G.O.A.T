---
type: module
status: implemented
tags: [module, utility, backend, paradigm]
---

# Utility AI (GOAT_Utility)

> **Status:** Implemented
> **Gem:** `GOAT_Utility`
> **Folder:** `Code/Source/Backends/Utility/`
> **Registers:** the `utility` decision backend
> **Log tag:** `GoatUtility` — turn it on with `EnableLog GoatUtility`

---

## What it is

Scoring. You list everything an agent could do, each choice says what makes it worth doing, and
the best score runs. Nothing is written in order of priority and nothing is written as a goal:
the ordering falls out of the numbers, every tick.

Put `utility` in an agent's **Brain** field to use it. It is an ordinary gem: delete it and the
rest of GOAT still builds.

**There are no response curves.** A consideration reads a float that is *already* scaled to 0
to 1 and uses it as it stands. Curves are the part of utility AI that needs a graph editor to be
usable at all, and there isn't one; so anything shaped is written as a few lines of Lua instead,
where you can see it and change it without a tool.

---

## The words

| Word | Main property | What it is |
| :--- | :--- | :--- |
| `utility` | name | the whole set of choices |
| `choice` | `name` | one thing the agent could do, and its steps |
| `consider` | `key` | a float in 0 to 1 arguing for the choice it sits in |

> **It is `choice`, not `option`.** `option` already belongs to declared `plan`s
> (`GOAT.lua`), and `GOAT_DeclareNode` leaves a word that exists alone rather than taking it
> over — so an `option` here would silently call the wrong function.

Properties on a `utility`:

| Property | Default | What it does |
| :--- | :--- | :--- |
| `recheck` | `0.25` | the **shortest** gap between scoring a running choice against the others |
| `momentum` | `0` | how much the running choice's score is raised by, to stop it flip-flopping |
| `pick` | `"best"` | `"best"` or `"weighted"` |
| `top` | — | how many of the best a weighted draw comes from; only with `pick = "weighted"` |

Properties on a `choice`:

| Property | Default | What it does |
| :--- | :--- | :--- |
| `combine` | `"multiply"` | `multiply`, `mean`, `min`, `max`, or a behaviour's name |
| `score` | — | a behaviour whose answer counts as one more consideration |
| `commit` | `false` | run the steps to the end whatever else starts scoring higher |

`condition`, `compare`, `wait`, `script`, `embed` and the module verbs are shared with every
other paradigm, so they work here unchanged.

---

## A program

```lua
behavior "Panic" {
    score = function(me, ctx, considered)
        local fear = considered[1] or 0.0
        return fear * fear
    end,
}

return utility "ExampleChoices" {
    recheck = 0.25,
    momentum = 0.15,
    pick = "weighted", top = 2,

    choice "Flee" {
        consider "fear",
        consider "health_low",
        score = "Panic",
        commit = true,
        wait(2.0),
    },

    choice "Fight" {
        consider "morale",
        combine = "mean",
        script "Swing",
        wait(0.5),
    },

    -- Worth a little, never worth a lot. See the trap below.
    choice "Idle" { consider "idle_worth", wait(1.0) },
}
```

A choice's body is a **plan**, not a tree: its steps run in order and the first failure ends it.
That is why `sequence` and `selector` are refused inside one — they are shapes a tree walks, and
a choice reaches a tree by embedding it.

---

## How a choice is scored

1. Every `consider` is read. A value outside 0 to 1 is clamped, and a value that cannot be read
   at all counts as zero — both reported **once**, not once per agent per frame.
2. Those values are folded by `combine`. **If the fold is zero the choice is out**, and nothing
   else about it runs.
3. If `score` names a behaviour, it is asked, and its answer joins the values.
4. If `combine` names a behaviour, it is handed every value and its answer is the score.

Step 2 is the reason a scorer is affordable: the cheap numbers rule a choice out before anything
reaches Lua. A choice whose `combine` is a behaviour has no fold to run first, so it is always
asked — if you want the cheap way out of one, use `score` instead.

The default is `multiply` because that is the only fold where one consideration can rule a choice
out on its own, which is usually what "no ammo" is meant to mean.

> **A choice with no considerations scores one, not zero.** Folding an empty set gives 1.0 —
> nothing argued for it, but nothing argued against it either — so a bare `choice "Idle"
> { wait(1.0) }` is the *highest* scoring thing in the program and wins almost everything. A
> fallback wants a low constant: declare a float with a default nobody writes and `consider` it.
> Then what idling is worth lives in the blackboard asset, where it can be tuned without
> touching a script.

---

## When it re-scores

Never on a clock of its own. A utility program is woken the way every other program is: something
wrote a blackboard scope it reads. `recheck` then throttles that from there.

> **`recheck` is a floor, not a period.** If nothing an agent considers ever changes, it never
> re-scores, forever. That is [[GuardWatch]] working exactly as intended — an idle agent costs
> nothing — but the word reads like an interval, so it is worth saying plainly.

A different winner interrupts the running choice, unless that choice said `commit = true`.

---

## Picking

`pick = "best"` takes the highest, with written order breaking a tie, so the same numbers always
answer the same way.

`pick = "weighted"` draws from the best `top` in proportion to their scores, which is what stops
a crowd of agents with the same blackboard doing the same thing at the same moment.

**Every agent draws from its own sequence**, seeded from its own handle. A single shared stream
is itself a way to move in step: a hundred agents drawing in the same tick take consecutive
values from one sequence. The draw is deterministic — the same agent replays the same run.

---

## Limits

| Constant | Value | Why |
| :--- | :--- | :--- |
| `MaxChoices` | 32 | choices in one program, so a scoring pass is a fixed size array |
| `MaxConsiderations` | 8 | a choice arguing from more is one nobody can reason about |
| `MaxChoiceSteps` | 16 | steps one choice runs |

An agent's whole state is a **16-byte** `UtilityCursor`: the choice it is on, what that scored,
how long since it last scored, and its own draw sequence. That is the smallest of the three
paradigms — `HtnPlanRecord` is 66 bytes.

**A utility program is never finished, only idle.** When nothing scores it asks to be woken again
rather than reporting a result, because a choice argues from numbers that move and asking later
can answer differently. So a program embedded in a tree or a domain is ended by *that* one's
guards, the way a `wait` is — it will not end itself.

---

## What it costs

`BM_TickScored` against `BM_TickBand`, 8 choices of 4 considerations each, on the same
population:

| | ns per agent |
| :--- | :--- |
| `BM_TickBand` (walking a tree) | 13 |
| `BM_TickScored` (scoring every choice, every tick) | 161 |

That is the **worst case on purpose**: the benchmark forces every agent to score every tick.
Dormancy and the `recheck` floor are what keep a real population away from it, and neither is
being measured there.

Two findings came out of writing that benchmark, and both are worth knowing if you extend this:
naming the variable a value came from means the blackboard *scans* for the name, so it is asked
for only when there is something to report; and where each scope is stored is the same answer for
every number a pass reads, so it is resolved once per pass rather than once per read. Together
they were 3.7x.

---

## Diagnostics

`EnableLog GoatUtility` in the console:

```
GOAT: agent 12 program 'Soldier' chose 'Fight' at 0.720 from 3 choice(s)
GOAT: agent 12 program 'Soldier' left 'Fight' at 0.310 for 'Flee' at 0.884
GOAT: agent 12 program 'Soldier' dropped 'Fight': nothing argues for it any more
GOAT: agent 12 program 'Soldier' has nothing worth doing
```

Authoring mistakes are refused when the program compiles, not found on the tick that needed them
— including a `combine` or `score` naming a behaviour nobody declared. The one caveat is that
this can only see what has already loaded, so a scorer declared in a script that loads *after*
the program compiles fails the compile. That is the right failure; it is loud and it names the
fix.

---

## Scoring, trees or task networks?

**A tree** is good when the structure is the design: priority order matters and you want to see
at a glance what beats what.

**A task network** is good when the goal is the design: several ways to achieve something, and
you want the planner to pick.

**Scoring** is good when there is no order and no goal — just a lot of things an agent could
reasonably do, whose relative merit is a matter of degree rather than of rank. Its weakness is
the mirror of the tree's strength: nothing about a program tells you what beats what, because
that is a property of the numbers at run time. Turn the log on.

It also makes a good *top* layer over the other two: a handful of choices, each embedding a tree
or a domain that knows how to carry it out.

---

## Related

- [[Behavior Trees]]
- [[Task Networks]]
- [[Mixing Paradigms]]
- [[IDecisionBackend]]
- [[Backend Abstraction Theory]]

---

*Last updated: 2026-08-29*
