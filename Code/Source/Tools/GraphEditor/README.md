# Graph editor

Not built yet. This folder is the seam it plugs into, so adding it later needs no runtime change.

## Why it drops in cleanly

Authoring and execution are already separated:

```
  guard.lua  ─┐                                    ┌─► DecisionProgram  (runtime)
              ├─►  AuthoredNode  ──► TreeCompiler
  guard.bt   ─┘    (reflected, serializable)
  (this tool)
```

`TreeCompiler` consumes the authored node tree, never Lua. The runtime cannot tell which front end
produced a tree, so a graph editor is an additional producer rather than a replacement. Lua and the
graph stay co-equal.

## Already in place

- `BehaviorTreeAsset` / `AuthoredNode` — reflected and serializable, with a
  `AuthoredNodeMetadata` field carrying node position and comment that the runtime ignores.
- `NodeTypeRegistry` — a descriptor per node type: display name, category, child arity, and a typed
  parameter list. This is what a palette and a property panel need. It already drives authoring
  validation and `goat_listNodes`.
- `TreeLibrary` — named trees plus rebindable slots, which is how `subtree` composition works.

## Still to do

- A `.bt` asset handler and builder, mirroring how `.bbx` is registered in `GOATSystemComponent`.
- A `GraphModel` graph plus `GraphCanvas` scene and node palette, built from `NodeTypeRegistry`.
- A Qt tool window registered from `GOATEditorSystemComponent`.
- Round tripping between a Lua authored tree and a graph.

## One constraint worth knowing

`Gems/GraphModel` cannot be used at runtime. Its CMake lives entirely inside
`PAL_TRAIT_BUILD_HOST_TOOLS` with no runtime target, its `Model/` and `Integration/` headers share
one file list, and `Model/Graph.h` includes `GraphCanvasMetadata.h`, which pulls in the
GraphCanvas/Qt tree. Its own header notes it is *"designed with primarily editor processing in mind,
rather than runtime processing"*.

So the graph model belongs to the editor target only, and it bakes into `AuthoredNode` for the
runtime — the same split ScriptCanvas and Landscape Canvas already use. `GraphCanvas` should be
added to `gem.json` dependencies only when this tool is actually built.
