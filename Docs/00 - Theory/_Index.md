---
type: index
status: active
tags: [theory, moc]
---

# Theory – Map of Content

This folder documents the **conceptual foundations** of G.O.A.T. It explains *why* the framework is designed the way it is, not just *what* it does. These notes are essential for anyone extending the framework or understanding its philosophy.

---

## 📂 Notes in this Folder

| Note | Description |
| :--- | :--- |
| [[Design Principles]] | The 6 core pillars that guide every architectural decision in G.O.A.T. |
| [[Backend Abstraction Theory]] | The unifying concept that all AI paradigms (BT, HTN, GOAP, Utility) are just `IBackend` implementations. |
| [[Performance Model]] | How G.O.A.T. achieves high performance through flat programs, tick bands, and interval services. |
| [[Extensibility Model]] | How modules add new actions, node types, and backends without modifying the core engine. |

---

## 🔗 Where to go next

- **Architecture:** Read [[Layered Overview]] to see how these theories manifest in the actual codebase.
- **Lua API:** Read [[Vocabulary]] to see how these principles translate into the authoring DSL.
- **Planning:** Read [[Planned Features]] to see how these theories guide future modules (Navigation, Bark, Perception).

---

## 📌 How to use this folder

- Each note follows the [[Theory Template]].
- Use `#theory` tags for filtering in Graph View.
- Link between theory notes using `[[Wikilinks]]`.

---

*Last updated: 2026-08-26*