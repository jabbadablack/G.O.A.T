---
type: index
status: active
tags: [modules, moc]
---

# Modules – Map of Content

This folder documents the **gems that ship beside the core**. Each one adds paradigms, verbs or
node types without the core knowing anything about it, and each can be turned off in a project's
`project.json`.

Two kinds live here. **Paradigm gems** decide how an agent thinks. **Module gems** add verbs for
a domain.

---

## 📂 Notes in this Folder

### Paradigms
| Note | Description |
| :--- | :--- |
| [[Behavior Trees]] | The `tree` backend. Composites, decorators, guards, services. |
| [[Task Networks]] | The `htn` backend. Tasks, methods, primitives, effects. |

### Modules
| Note | Description |
| :--- | :--- |
| [[Navigation]] | Movement verbs: `move_to`, `is_at_location`, `does_path_exist`. |
| [[Smart Objects]] | Props advertising what they can be used for. |
| [[Animation]] | Verbs for driving animation from a program. |
| [[Bark]] | Planned. Trigger-volume based social reactions. |

---

## 🔗 Where to go next

- **Planned Features:** Read [[Navigation Library]] for the theoretical design of the navigation module.
- **Extensibility:** Read [[Extensibility Model]] to understand how modules are registered.
- **Guides:** Read [[Adding New Actions]] to learn how to contribute to a module.

---

## 📌 How to use this folder

- Each note follows the [[New Module Template]].
- Use `#module` tags for filtering in Graph View.
- Link between module notes using `[[Wikilinks]]`.

---

*Last updated: 2026-08-26*