---
type: index
status: active
tags: [architecture, moc]
---

# Architecture – Map of Content

This folder documents the **structural foundation** of G.O.A.T. It maps out the layered design, the data pipeline from authoring to execution, and the blackboard system that ties everything together.

---

## 📂 Notes in this Folder

| Note | Description |
| :--- | :--- |
| [[Layered Overview]] | High-level 3-layer architecture (Lua, C++ Core, Runtime Components) |
| [[Data Flow]] | The full pipeline from Lua tree to ActionPlan execution |
| [[Blackboard System]] | How variables are declared, stored, and accessed across scopes |

---

## 🔗 Where to go next

- **Theory:** Read [[Design Principles]] to understand *why* these architectural choices were made.
- **C++ Components:** Read [[GOATSystemComponent]] to see how these layers are wired together in code.
- **Lua API:** Read [[Vocabulary]] to see how the architecture exposes itself to designers.

---

## 📌 How to use this folder

- Each note follows the [[Component Template]] (where appropriate).
- Use `#architecture` tags for filtering in Graph View.
- Link between architecture notes using `[[Wikilinks]]`.

---

*Last updated: 2026-08-26*