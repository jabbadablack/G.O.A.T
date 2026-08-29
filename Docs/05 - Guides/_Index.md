---
type: index
status: active
tags: [guides, tutorial, moc]
---

# Guides – Map of Content

This folder contains **step-by-step tutorials** for extending and using G.O.A.T. Each guide walks you through a specific task, from writing a custom backend to creating a Director AI.

---

## 📂 Notes in this Folder

| Note | Description |
| :--- | :--- |
| [[Writing Custom Backends]] | How to create a C++ or Lua planning backend and register it. |
| [[Adding New Actions]] | How to create a C++ `IActionState` (like `MoveTo`) and use it in Lua. |
| [[Creating a Director AI]] | How to set up a global entity with `Band=3` and a `delegate` node to orchestrate waves. |
| [[Mixing Paradigms]] | How one program hands work to another, and when to reach for `subtree`, `delegate` or `embed`. |

---

## 🔗 Where to go next

- **Theory:** Read [[Extensibility Model]] to understand the interfaces these guides use.
- **C++ Components:** Read [[ActionStateRegistry]] to see how actions are stored.
- **Lua API:** Read [[Backends]] to see how backends are authored in Lua.

---

## 📌 How to use this folder

- Each note follows the [[Guide Template]].
- Use `#guide` and `#tutorial` tags for filtering in Graph View.
- Link between guide notes using `[[Wikilinks]]`.

---

*Last updated: 2026-08-26*