---
type: guide
status: active
tags: [guide, tutorial, how-to]
---

# Creating a Director AI

> **Time:** about 20 minutes
> **You should have read:** [[Director AI]], [[Behavior DSL]]

We'll build a director that watches a crowd and calls them in when it decides to. It starts
governing the whole level, and then we narrow it down.

---

## Step 1 — Make the entity

Create an entity, call it `Crowd Marshal`, and add a **GOAT Director** component.

Don't add a GOAT Agent as well. A director already is an agent, and the two components refuse to
sit together for exactly that reason.

Fill in:

- **Scripts** — the Lua file we're about to write.
- **Blackboards** — any `.bbx` declaring variables your program uses. The `director_*` variables
  are declared from C++, so you don't need one for those.
- **Programs** — the name of the program it runs. The first one listed is where it starts.
- **Brain** — `tree` or `htn`. A director is an ordinary agent, so either works.
- **Detail** — leave it at 3. That's once a second, which is the right rate for something making
  strategic decisions. Reconsidering strategy every frame just makes it thrash.

At this point it governs every other agent in the level. That's the default, and often it's what
you want.

---

## Step 2 — Write what it does

```lua
-- CrowdMarshal.lua
local crowdRallying

behavior "CallMuster" {
    tick = function(me, ctx)
        crowdRallying = crowdRallying or ctx:Key("crowd_rallying")

        me.tick = (me.tick or 0) + 1

        -- Gather them, hold them a while, then let them go.
        local phase = me.tick % 40
        ctx:SetBool(crowdRallying, phase >= 10 and phase < 22)
        return SUCCESS
    end,
}

return tree "CrowdMarshal" {
    selector {
        sequence {
            condition "crowd_rallying",
            order_interrupt "CrowdRally" { limit = 6 },
        },
        script "CallMuster",
    },
}
```

Two things worth noticing.

`condition "crowd_rallying"` isn't only a test. A declared condition observes the variable it
reads, so the moment anything writes `crowd_rallying` this director reconsiders. You don't
write `abort` to get that; it's the default.

`limit = 6` caps how many agents one step touches. Without it the whole crowd flips at once,
which reads as a glitch rather than a gathering.

---

## Step 3 — Check it works

Run the level and open the console:

```
ListDirectors
```

You'll get a line per director with its priority, what's narrowing it, and how many agents it
governs. With nothing attached yet it says `narrowed by nothing`.

```
DumpDirector <entityId>
```

lists exactly which agents, one per line.

---

## Step 4 — Narrow it to an area

Right now the marshal reaches the whole level. Let's give it a patch of ground.

1. Add a **Sphere Shape** to the entity and set its radius to something like 30.
2. Add a **GOAT Director Area** component.

That's it — there's nothing to configure. The area filter uses whatever shape is on the entity,
which is why it won't let you add it without one.

Run `DumpDirector` again and the list is shorter. Move the entity and the area moves with it.

A sphere is the simple case. Use a **Box Shape** or a **Polygon Prism** when you want a zone with
an actual shape — a courtyard, a corridor, a room.

> **Want the zone to stay put while the director roams?** Put the shape on its own entity and
> point the filter at it from code with
> `GOATDirectorAreaFilterRequestBus::Event(filterEntity, &...::SetShapeEntity, zoneEntity)`.
> There's deliberately no field for this in the property editor — the common case is the shape
> sitting right there, and requiring it is what keeps that case correct.

---

## Step 5 — Narrow it to a squad or a tag

Add a **GOAT Director Squad** component. It has two lists:

- **Squads** — squad names, matched against the squad each agent joined.
- **Tags** — tags, read from the stock LmbrCentral **Tag** component on the agent's entity.

The two lists are an **OR**. `Squads: [Alpha]` and `Tags: [wounded]` governs everyone in Alpha,
plus anyone tagged wounded whatever squad they're in.

Leave both empty and it narrows nothing, so an unfinished component can't quietly strip a
director of everyone it governs.

To tag an agent, add a **Tag** component to its entity and type the tag in. Nothing in GOAT needs
to know about it.

---

## Step 6 — Stack them

Filters combine with **AND**. With both components attached, the marshal governs agents that are
inside the sphere *and* in squad Alpha.

If you want an OR across filters — "squad Alpha, or anyone in the plaza, whichever" — make two
directors. Priority decides who wins where they overlap.

---

## Prefer publishing to ordering

The example above orders, because that's what a guide about directors should show. In real use,
publishing is usually better.

An order stops what an agent was doing. Publishing a variable doesn't — agents read it and each
decides for itself, and because a declared condition observes its key, they notice immediately.
One write covers a hundred agents. No reach is walked, and nothing gets interrupted:

```lua
behavior "PaceTheCrowd" {
    tick = function(me, ctx)
        crowdPace = crowdPace or ctx:Key("crowd_pace")
        ctx:SetNumber(crowdPace, me.pace)
        return SUCCESS
    end,
}
```

Reach for `order_*` when you genuinely need an agent to drop what it's doing. Reach for a
variable the rest of the time.

---

## When it isn't working

**It governs nobody.** Check `DumpDirector`. If a filter is listed and the count is zero, that
filter is rejecting everyone — usually a squad name that doesn't match, or a shape smaller than
you thought.

**It governs everyone despite an area filter.** The shape is missing or its entity is inactive.
Filters fail open on purpose: a filter that can't answer accepts, so a broken setup shows up as
one warning instead of a director that silently does nothing. Check the log for
`has no shape to filter a director's reach by`.

**Orders are ignored.** Look at `director_refused`. It's usually cooldown. Either wait it out or
lower **Cooldown** on the director.

**Nothing happens at all.** Make sure you used GOAT Director and not GOAT Agent — the order verbs
only exist for directors.

---

## Related

- [[Director AI]]
- [[GOATDirectorComponent]]
- [[IDirectorFilter]]
- [[Behavior DSL]]

---

*Last updated: 2026-08-27*
