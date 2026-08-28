---
type: theory
status: implemented
tags: [theory, director, architecture]
---

# Orchestration Patterns

> **Status:** Implemented
> **Core files:** `Code/Source/Clients/GOATDirectorComponent.cpp`, `Code/Source/Core/Director/DirectorActions.cpp`

---

## The problem

Games need a brain above the individual NPC. Waves that escalate, difficulty that scales, a crowd
that gathers when something happens. Without a pattern for it, that logic ends up scattered across
a dozen components that each poke at agents directly.

GOAT's answer is a [[Director AI]] — an agent whose leaves act on *other* agents. It is not a new
system. It runs an ordinary program on an ordinary agent; only its verbs are different.

---

## Two patterns, and one is usually right

Everything below is a variation on one choice.

### Publish

The director writes a blackboard variable and stops. Agents that read it react on their own.

```lua
primitive "Adjust" { script "PaceTheCrowd" }   -- writes crowd_pace, names nobody
```

- **Costs one write**, however many agents there are.
- **Interrupts nothing.** Each agent decides what to do about it, when it next ticks.
- Works because a declared `condition` observes its key, and [[GuardWatch]] counts changes per
  scope rather than registering callbacks.

### Command

The director puts agents onto another program.

```lua
primitive "Order" { order_interrupt "CrowdRally" { limit = 6 } }
```

- **Costs a walk of the reach**, and stops what those agents were doing.
- Needs rationing: `limit` per step, a cooldown per director, and a reach it cannot exceed.

**Prefer publishing.** Command when you genuinely need an agent to drop what it is doing right
now. This is the whole argument the director design rests on, and the Scry test bench ships one
director of each kind side by side to make the contrast concrete.

---

## Intensity over time

The most useful director in the literature is also the simplest: walk a number up and down and let
everyone read it. Left 4 Dead's director is, at heart, one intensity value.

```lua
if me.rising then
    me.pace = me.pace + 1
    if me.pace >= 3 then me.rising = false end
else
    me.pace = me.pace - 1
    if me.pace <= 0 then me.rising = true end
end
ctx:SetNumber(crowdPace, me.pace)
```

That is twenty lines of Lua on a global variable. It is **content, not machinery** — which is why
nothing like it exists in the C++ core, and should not.

---

## Sense, act, report

A director's program usually has three kinds of step:

| Step | Does | Example |
| :--- | :--- | :--- |
| **Sense** | reads the world, writes what it concluded | `script "CallMuster"` |
| **Act** | publishes or commands | `order_interrupt "CrowdRally"` |
| **Report** | reads back what the last act achieved | `script "ReportMuster"` |

Reporting is worth doing. Every order writes `director_reach`, `director_changed` and
`director_refused`, and those are ordinary variables — so a method or a branch can condition on
"my last order was refused by everyone" using the words it already has.

---

## Several directors

Directors are not a hierarchy. Several can govern the same agent, and **priority** settles who
wins. That is how you compose:

- A global pacer at low priority, publishing intensity to the whole level.
- A local marshal at high priority, narrowed to one area, commanding within it.

Filters combine with AND *within* one director, so two directors is also how you express an OR:
"squad Alpha, or anyone in the plaza" is two directors, not one clever filter.

---

## What not to do

**Don't poll.** A service checking a variable every 0.3 s is what reactivity replaced. A
`condition` notices immediately and costs nothing while nothing happens.

**Don't order every tick.** Switching a program stops whatever the agent was doing. Ordering one
every tick leaves an agent permanently restarting and never finishing anything. That is what the
cooldown defaults to five seconds for.

**Don't make the director govern everything by accident.** A director with no filters governs the
whole level. That is a fine default for a pacer and a terrible one for a marshal.

**Don't reach for a director first.** Most coordination is better as a shared variable that agents
read. A director is for decisions no single agent can make.

---

## Related

- [[Director AI]]
- [[Creating a Director AI]]
- [[Blackboard System]]
- [[GuardWatch]]
- [[Design Principles]]

---

*Last updated: 2026-08-27*
