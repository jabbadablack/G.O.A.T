# Graph editor

The **GOAT Program Editor**, under *Tools* in the O3DE Editor. It authors `.goat` files, which are
`ProgramAsset`s holding an `AuthoredNode` root.

A program is made in the **Asset Browser** — right click a folder, *Create*, *GOAT Program* — which
writes the file and opens it here. Double clicking a `.goat` opens it here too. `ProgramAsset`
deliberately does not set `EnableForAssetEditor`: a program is a graph, not a property grid.

## Where it sits

Authoring and execution are separate, so this is an additional producer rather than a replacement:

```
  guard.lua  ─┐                                    ┌─► DecisionProgram  (runtime)
              ├─►  AuthoredNode  ──► TreeCompiler
  guard.goat ─┘    (reflected, serializable)
  (this tool)
```

`TreeCompiler` consumes the authored node tree, never Lua. The runtime cannot tell which front end
produced a tree, so Lua and the graph stay co-equal.

## It knows no paradigm

There is one node class, `ProgramNode`, for every word. Its slots are built from the
`NodeTypeDescriptor` the registry holds: the parameter list becomes property rows on the node face,
and `NodeKind` becomes the child slot's arity, so a leaf cannot be given a child at all. A backend
that registers a word gets a palette entry and a node without touching this folder — which is how
`domain`, `choice` and `move_to` appear beside `selector`.

`NodeTypeDescriptor::m_backend` says which paradigm owns a word. It decides the node's colour, and
it is what stops a `task` from being dropped into a behaviour tree.

## The parts

| File | Does |
| :--- | :--- |
| `GraphContext` | One GraphModel data type per `BlackboardType`, plus the execution edge. |
| `ProgramNode` | One node class for every word, slots built from the registry. |
| `ProgramNodePaletteItem` | A palette row and the mime event carrying which word to create. |
| `ProgramGraphSerializer` | `AuthoredNode` ⇄ graph. |
| `ProgramValidator` | Per node checks, then the real backend `Compile`. |
| `MainWindow` | The window, the File actions and undo. |

## Two things worth knowing

**Sibling order is vertical.** GraphCanvas lays a graph out left to right, so a child sits to the
right of its parent and execution order reads top to bottom. Moving a node up or down changes what
runs first, which is why validation re-runs on a move and not only on an edit.

**Undo is a whole-graph snapshot.** GraphCanvas batches undo points and hands them to the client;
it stores nothing itself. `MainWindow` serialises the whole `GraphModel::Graph` per point. Note that
`Slot` caches a `shared_ptr` back to its owning `Node`, so a graph being dropped needs
`Graph::ClearCachedData()` first or it holds itself alive.

## Watching a running agent

`AgentDebugSource` is where agent state is read from. `LocalAgentDebugSource` reads the agent
system in this process; a remote one asks a launcher. The panels are written against the
interface so that adding the second is a transport rather than a second copy of the tool.

`AgentBrowserPanel` lists what a poll returned. It only rebuilds its rows when the set of agents
changes, because resetting a model drops the selection and ten polls a second would make an
agent impossible to hold on to.

`RunningHighlight` outlines the path an agent is on, as a GraphCanvas graphics effect rather
than a palette override. An effect is not part of the graph, so it neither reports the program
as modified -- which would ask for validation, which paints, which asks again -- nor fights the
red a failing node already carries.

## Not built yet

- A blackboard inspector (phase 3).
- Viewport debug draw (phase 4).
