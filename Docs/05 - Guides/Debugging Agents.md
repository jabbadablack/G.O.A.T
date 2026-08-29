---
type: guide
status: active
tags: [guide, tools, debugging]
---

# Debugging Agents

The GOAT Program Editor is not only for writing programs. Open one, pick an agent, and the path
it is running lights up on the same canvas you authored it on.

---

## Watching an agent

1. Open **Tools → GOAT Program Editor**.
2. The **Agents** panel on the right lists every registered agent: which entity it drives, which
   program it is running, on which backend, its pacing band, its squad, the verb in flight, and
   how far through its plan it is.
3. Click one. Its program opens read-only, and the nodes it is currently inside are outlined in
   green, root first down to the leaf that is actually running.

Agents exist as soon as a level has them — you do not have to enter game mode first, though
pressing **Ctrl+G** will start them acting. When the list is empty the panel says why.

The program a running agent opens is the one the runtime compiled, read back through
`IAgentSystem::EmitProgram`. It is read-only, because what is on screen is a program that is
running rather than a file being edited.

---

## Reading the highlight

The outline is a **graphics effect**, not a colour change on the node. That matters in two ways:

- It sits alongside validation. A node can be red because it is wrong and outlined because it is
  running, and you can see both at once.
- It does not modify the graph, so watching an agent never marks a program dirty.

A node that is running inside an **inlined subtree** is not on the canvas you are looking at, so
nothing is lit for it. That is deliberate: a `subtree` node compiles to nothing and the tree it
names is spliced in where it stood, so the nodes running there belong to another program. Open
that program to watch it. See [[ProgramNodeRef]].

---

## What it costs

Nothing, when nobody is watching.

There is no trace, no ring buffer and no per-agent recording. The editor asks, ten times a
second, where each agent is; each backend answers by reading the state block it already keeps —
a behaviour tree's active leaf, a task network's current plan step, a utility program's chosen
option. Close the window and no work happens at all.

The trade is that the panel shows the present and not the past. A node that succeeds and is left
between two polls is never seen. What is running now is always right.

Agents on slower pacing bands are only asked to decide every 100, 250 or 1000 milliseconds
([[Performance Model]]), so a distant agent's row changes slowly because the agent does, not
because the tool is behind.

---

## From the console

Everything the panel shows is also available as text, which is what to reach for in a launcher:

```
GOATSystemComponent.ListAgents
GOATSystemComponent.DumpAgent <entityId>
GOATSystemComponent.DumpPlan <name>
GOATSystemComponent.ListSquads
GOATSystemComponent.DumpDirector <entityId>
GOATSystemComponent.DumpVariable <name> [entityId]
```

`ListAgents` and the panel are built from the same [[AgentSnapshot]], so the two cannot drift
apart. If they ever disagree, that is a bug.

---

## Related Notes

- [[AgentSnapshot]]
- [[ProgramNodeRef]]
- [[Performance Model]]
