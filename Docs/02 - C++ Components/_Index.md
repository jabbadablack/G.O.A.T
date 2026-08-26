---
type: index
status: active
tags: [cpp, core, moc]
---

# C++ Components – Map of Content

This folder documents the **core C++ classes** that make up G.O.A.T. Each note covers a specific component, its responsibilities, interfaces, and dependencies.

---

## 📂 Notes in this Folder

| Note | Description |
| :--- | :--- |
| [[GOATAgentComponent]] | The single component that turns an entity into an AI agent. |
| [[GOATSystemComponent]] | The "God object" that initializes all services and owns the registries. |
| [[TreeCompiler]] | Compiles authored Lua trees into flat `DecisionProgram`s. |
| [[TreeWalker]] | Iteratively executes the compiled program, emitting `Intent`s. |
| [[LuaDispatch]] | Bridges C++ to Lua, calling `GOAT_Dispatch`, `GOAT_EmitTree`, etc. |
| [[LuaTreeBuilder]] | Reconstructs a `BehaviorTreeNode` hierarchy from flat Lua calls. |
| [[LuaPlanBuilder]] | Builds an `ActionPlan` from steps returned by a Lua backend. |

---

## 🔗 Where to go next

- **Architecture:** Read [[Layered Overview]] to see how these components fit into the whole.
- **Lua API:** Read [[Vocabulary]] to see how designers interact with these systems.
- **Guides:** Read [[Writing Custom Backends]] to learn how to extend these components.

---

## 📌 How to use this folder

- Each note follows the [[Component Template]].
- Use `#cpp` and `#core` tags for filtering in Graph View.
- Link between component notes using `[[Wikilinks]]`.

---

*Last updated: 2026-08-26*