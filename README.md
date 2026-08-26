# 🐐 G.O.A.T. – Gameplay Oriented Agent Toolkit

**A Backend-Driven AI Framework for O3DE**  
*Lua-First Authoring. C++ Performance. Unlimited Planning.*

---

## 📖 Overview

**G.O.A.T.** is a production-ready AI framework for Open 3D Engine that unifies Behavior Trees, HTN/GOAP planning, Utility AI, and Director AI under a single, elegant interface.

Instead of locking you into one AI paradigm, it uses a **Backend Abstraction**: tree leaves simply emit `Intents`, and any `Backend` (Direct, Lua, GOAP, HTN) can turn those intents into `ActionPlans`.

- 🧠 **Lua-First Authoring:** Write entire trees, custom control flows, and planning backends in Lua.
- ⚡ **High Performance Core:** Flat, cache-friendly `DecisionPrograms` with zero string lookups at runtime.
- 🏗️ **Extensible Architecture:** Add new actions, node types, or entire planning algorithms via clean C++ interfaces.

---

## 📚 Documentation (Required Setup)

**All project documentation lives in the `Docs/` folder.**

We use **[Obsidian](https://obsidian.md)** as our documentation platform. Obsidian is a local-first Markdown editor that supports **wikilinks**, **backlinks**, **Mermaid diagrams**, and **graph views** – all of which are essential for navigating this project's complex architecture.

### Why Obsidian?

- **Wikilinks** (`[[ComponentName]]`) – Click through cross-references instantly.
- **Mermaid Diagrams** – Visualize architecture, data flow, and hierarchy directly in the notes.
- **Graph View** – See how all components, theories, and guides connect.
- **Local Files** – Everything is plain Markdown, so it works with Git and any editor.

---

## 🚀 Getting Started with Obsidian

1. **Install Obsidian**  
   Download it from [obsidian.md](https://obsidian.md) (free for personal/commercial use).

2. **Open the Vault**  
   - Launch Obsidian.
   - Click **"Open folder as vault"**.
   - Select the `Docs/` folder in the root of this repository.

3. **Start Here**  
   - Open `Docs/Home.md` – it is the central Map of Content.
   - From there, follow the links to **Theory**, **Architecture**, **Components**, **Lua API**, and **Guides**.

### 💡 Tips for Using the Vault

- Use **Ctrl/Cmd + P** to quickly search and open any note (`[[Wikilinks]]` will resolve if the file exists).
- Open the **Graph View** (top right icon) to see the entire documentation network.
- Use **Backlinks** (right sidebar) to see which other notes reference the one you're reading.
- Follow the `_Index.md` files in each folder – they act as Maps of Content for that section.

---

## 🗂️ Project Structure (Quick Look)

    G.O.A.T/
    ├── Assets/        # Lua scripts, Editor Icons, Example behaviors
    ├── Code/          # C++ source, headers, tests, CMake files
    ├── Docs/          # Obsidian Vault (Project Documentation) ← READ THIS
    ├── Registry/      # Asset Processor settings
    └── README.md      # You are here

---

## 📌 Current Status

- ✅ Core Behavior Tree Engine
- ✅ Lua DSL Authoring Vocabulary
- ✅ Direct & Lua Backend System
- ✅ Blackboard System (Global/Agent/Squad)
- ❌ Navigation Library (Planned – see `Docs/06 - Planned Features/Navigation Library.md`)
- ❌ Bark System (Planned)
- ❌ Perception Module (Planned)

---

## 🔗 Quick Links to Key Docs

- **[Open the Docs Vault](Docs/Home.md)**
- [Architecture Overview](Docs/01%20-%20Architecture/Layered%20Overview.md)
- [Lua Vocabulary](Docs/03%20-%20Lua%20API/Vocabulary.md)
- [C++ Components Index](Docs/02%20-%20C%2B%2B%20Components/_Index.md)

---

*© 2026 G.O.A.T. Project. Built for O3DE.*