---
type: index
status: active
tags: [lua, authoring, moc]
---

# Lua API – Map of Content

This folder documents the **Lua authoring vocabulary** that designers use to create AI behavior. It covers the DSL functions, node constructors, and patterns for writing trees and backends.

---

## 📂 Notes in this Folder

| Note | Description |
| :--- | :--- |
| [[Vocabulary]] | All global functions (`tree`, `behavior`, `flow`, `backend`) and node constructors (`selector`, `wait`, `script`). |
| [[Behavior DSL]] | Annotated examples of how to author behavior trees in Lua. |
| [[Flows]] | How to write custom composites and decorators using the `flow` function. |
| [[Backends]] | How to write a full planning backend in Lua using `backend "Name" { plan = ... }`. |

---

## 🔗 Where to go next

- **Theory:** Read [[Design Principles]] to understand *why* Lua is the authoring language.
- **C++ Components:** Read [[LuaDispatch]] to see how Lua connects to C++.
- **Guides:** Read [[Creating a Director AI]] to see practical Lua usage.

---

## 📌 How to use this folder

- Each note follows the [[Asset Template]] (where appropriate).
- Use `#lua` and `#authoring` tags for filtering in Graph View.
- Link between Lua notes using `[[Wikilinks]]`.

---

*Last updated: 2026-08-26*