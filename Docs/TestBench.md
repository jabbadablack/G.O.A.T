# The GoatTest bench

`Scry`'s `GoatTest` level carries a bench for the parts of the gem that only fail at runtime.
Seven directors sit in it doing nothing until you ask for a test:

```
GOATSystemComponent.SetVariable test_mode 2
```

Mode `0` is off, which is where they start. One mode at a time keeps the log readable.

Turn the channels on first:

```
LoggerSystemComponent.EnableLog GoatDirector
LoggerSystemComponent.EnableLog GoatAgent
```

The bench is five agents — `Alpha 1`, `Alpha 2` (near), `Alpha 3` (far), `Beta 1`, `Beta 2` — each
starting in `Standing` and also listing `Styled`, which is the tree with an unbound subtree slot.

---

## Before any mode: what should already be true

**The squad guard fires.** `Standing` guards on `squad_alerted`, which is **Squad** scoped.

```
GOATSystemComponent.SetVariable squad_alerted 1 <entityId of Alpha 1>
GOATSystemComponent.DumpVariable agent_marked  <entityId of Alpha 2>
```

Alpha 2 must become marked, because the write reached the squad's storage and woke it. Before the
observer fix this could never happen: an agent's guards were armed before it had squad storage to
watch, so a squad-scoped guard watched nothing for the whole level. Beta must stay unmarked.

**A slot tree cannot compile.** On entering game mode, five warnings say

> `Tree 'Styled': No tree is bound to subtree slot 'combat_style'`

That is not a bug in the bench — it is the proof that `subtree { tag = ... }` had never once run.

---

## Mode 2 — priority, and the tie rule

Two directors govern squad Alpha: priority 1 orders `Holding`, priority 9 orders `Rallying`.

```
GOATSystemComponent.SetVariable test_mode 2
GOATSystemComponent.ListAgents
```

The Alphas must be on **Rallying**, and `GoatDirector` must show the priority-1 command refused,
naming both numbers.

**Then swap which ticks first** — change the two directors' Detail sliders so their bands differ,
and re-enter. `Rallying` must still win. That is what proves the incumbent holds ties: with `<`
instead of `<=`, the winner would follow tick order instead of priority.

`Test Director (bad order)` also runs in this mode, against squad Beta, ordering a tree that does
not exist. What matters is what happens **after**: the Betas must still be commandable in mode 4.
If the pending priority were not cleared when the failed switch was applied, they never would be.

## Mode 4 — idempotence and cooldown

```
GOATSystemComponent.SetVariable test_mode 4
```

`Nagging` orders `Holding` on squad Beta every tick, with an eight second cooldown. Expect **one**
switch, then silence. `DumpAgent` on a Beta must show its elapsed time *continuing*, not resetting
— that is what proves the agents were not being stopped and restarted. The `selector` falling
through to `wait` is the refusal being reported.

## Mode 5 — reach, and that it narrows

```
GOATSystemComponent.SetVariable test_mode 5
GOATSystemComponent.DumpDirector <entityId of Test Director (reach)>
```

It starts as squad `Alpha` **and** radius 10, so it lists **Alpha 1 and Alpha 2** — not Alpha 3,
which is 18 m away. Now edit its fields in the property grid and re-enter:

| Change | Must list |
|---|---|
| Radius → 0 | all three Alphas |
| Squad → empty as well | every agent in the level |
| Tree → `Standing` | only agents still in that tree |

**Every removal must strictly widen.** If any removal *narrows*, the filters are combining with OR
rather than AND.

## Mode 8 — the three scopes

```
GOATSystemComponent.SetVariable test_mode 8
```

The scopes director governs everyone and writes one variable of each scope. Then:

```
GOATSystemComponent.DumpVariable threat_level
GOATSystemComponent.DumpVariable squad_order  <entityId of Alpha 1>
GOATSystemComponent.DumpVariable squad_order  <entityId of Beta 1>
GOATSystemComponent.DumpVariable agent_marked <entityId of Alpha 1>
```

`threat_level` is Global, so one write reaches everyone — including agents outside any reach.
`squad_order` is Squad, so Alpha and Beta each got it once, not once per member. `agent_marked` is
Agent, so it landed on each agent separately. Nothing said which; the variable's declared scope did.

## Mode 9 — bands

```
GOATSystemComponent.SetVariable test_mode 9
GOATSystemComponent.ListAgents
```

The Betas move to band 3 and visibly slow to once a second. This is the first time `SetBand` has
ever run in this gem.

## Rebinding, which needs no mode

```
GOATSystemComponent.RebindSubtreeCommand combat_style Rallying
GOATSystemComponent.ListTrees
```

`Styled` must now appear, having failed to compile at level start. That path matters: a tree whose
slot was unbound is not in the compiled set at all, so a rebind has to reach back to the trees Lua
declared rather than only the ones that compiled.

Then order an agent onto it and watch it run the rebound branch:

```
GOATSystemComponent.SetAgentTreeCommand <entityId of Alpha 1> Styled
```

Rebind to a name that does not exist and confirm `Styled` survives — a bad rebind must leave the
world running.

## The navigation filter

A `Wall` sits in the middle of the patrol area. Set a director's **Filter** to `path_distance` and
give it a radius that spans the wall: an agent on the far side must drop out of a reach that still
contains it geometrically. With `GOAT_Navigation` disabled the name resolves to nothing, and the
director warns and falls back to straight line rather than governing nobody.

```
GOATSystemComponent.ListReachFilters
```

## Stale references

With mode 5 running, delete one of the Alphas in game mode. The director must keep working, and a
director script holding that entity from a previous tick must read back nothing rather than
another agent's state — the roster compacts when an agent goes, so a remembered *position* would
quietly name somebody else.
