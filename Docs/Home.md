# 🐐 G.O.A.T. – Gameplay Oriented Agent Toolkit

**Welcome to the documentation vault for G.O.A.T.**  
This is the central hub for understanding the architecture, theory, and usage of the framework.

---

## 📌 Quick Links

| Section                                                                | Description                                                            |
| :--------------------------------------------------------------------- | :--------------------------------------------------------------------- |
| 🧠 **[00 - Theory](00%20-%20Theory/_Index.md)**                        | Design principles, backend abstraction, performance model              |
| 🏗️ **[01 - Architecture](01%20-%20Architecture/_Index.md)**           | Layered overview, data flow, blackboard system                         |
| ⚙️ **[02 - C++ Components](02%20-%20C++%20Components/_Index.md)**  | Core C++ classes and their responsibilities                            |
| 📜 **[03 - Lua API](03%20-%20Lua%20API/_Index.md)**                    | Authoring vocabulary, behavior DSL, flows, backends                    |
| 🧩 **[04 - Modules](04%20-%20Modules/_Index.md)**                      | The gems that ship beside the core: paradigms and verb modules         |
| 🛠️ **[05 - Guides](05%20-%20Guides/_Index.md)**                       | Step‑by‑step tutorials for extending the framework                     |
| 🗺️ **[06 - Planned Features](06%20-%20Planned%20Features/_Index.md)** | Theoretical designs for missing modules (Navigation, Bark, Perception) |
| 📅 **[07 - Changelog](07%20-%20Changelog/README.md)**                  | Version history and release notes                                      |

---

## 📖 What is G.O.A.T.?

G.O.A.T. is a **genre-neutral NPC AI framework** for O3DE. You write the AI in Lua; C++ provides
the infrastructure underneath it.

The core knows about agents, a blackboard, plans and actions — and nothing about how decisions get
made. A **paradigm** is a gem you can add or delete:

| Paradigm | Brain | Gem | Status |
| :--- | :--- | :--- | :--- |
| Behaviour trees | `tree` | GOAT_BehaviorTree | shipped |
| Task networks (HTN) | `htn` | GOAT_Htn | shipped |
| GOAP, Utility AI, … | yours | yours | write one, see [Writing Custom Backends](05%20-%20Guides/Writing%20Custom%20Backends.md) |

Both can run in the same level, on different agents, at the same time. They coordinate through
blackboard variables and neither knows the other exists.

A **director** is an agent whose leaves act on other agents. It is not a paradigm — it works with
whichever one you picked.

---

## 🚦 Current Status

| Feature | Status |
| :--- | :--- |
| Behaviour tree backend (`tree`) | ✅ Implemented, as its own gem |
| Task network backend (`htn`) | ✅ Implemented, as its own gem |
| Lua authoring DSL | ✅ Implemented |
| Blackboard (Global / Agent / Squad) | ✅ Implemented |
| Reactivity — a `condition` is a dependency | ✅ Implemented |
| Directors, with area and squad/tag filters | ✅ Implemented |
| Custom control flow (flows) | ✅ Implemented |
| `delegate` planners in Lua | ✅ Implemented |
| Navigation module | ✅ Implemented |
| Smart objects | ✅ Implemented |
| Animation module | ✅ Implemented |
| Graph editor | 🚧 Asset format ready, tool not built |
| Bark system | ❌ Planned |
| Perception module | ❌ Planned |

---

## 🚀 Getting Started

1. **Read the [Theory](00%20-%20Theory/_Index.md)** – understand the design philosophy.
2. **Explore the [Architecture](01%20-%20Architecture/_Index.md)** – see how the system fits together.
3. **Learn the [Lua API](03%20-%20Lua%20API/_Index.md)** – start writing your first agent.
4. **Consult the [Guides](05%20-%20Guides/_Index.md)** – when you need to extend the framework.

---

## 🗺️ Documentation Map

This vault is organized into **logical layers**:

| Folder | Purpose |
| :--- | :--- |
| `00 - Theory` | The *why* – conceptual foundations and design principles |
| `01 - Architecture` | The *what* – high‑level structure and data flow |
| `02 - C++ Components` | The *code* – detailed documentation of C++ classes |
| `03 - Lua API` | The *user manual* – how to author agents in Lua |
| `04 - Modules` | The paradigm and verb gems that ship beside the core |
| `05 - Guides` | How‑to tutorials |
| `06 - Planned Features` | Theoretical designs for future work |
| `07 - Changelog` | Release history |

---

## 💡 Using Obsidian Features

- `[[Wikilinks]]` – Click any highlighted note name to navigate directly.
- **Tags** – Files are tagged with `#theory`, `#core`, `#lua`, `#planned`, etc., for filtering in Graph View.
- **Backlinks** – Use the Backlinks pane to see which notes reference a given page.

---

*© 2026 G.O.A.T. Project. Built for O3DE.*