# 🐐 G.O.A.T.

### Gameplay Oriented Agent Toolkit

**Status:** Pre-Alpha  
**Engine:** O3DE  
**Language:** C++ / Lua  

---

## 📖 Overview

**G.O.A.T.** is a production-ready AI framework for Open 3D Engine that unifies Behavior Trees, HTN/GOAP planning, Utility AI, and Director AI under a single, elegant interface.

Instead of locking you into one AI paradigm, it uses a **Backend Abstraction**: tree leaves simply emit `Intents`, and any `Backend` (Direct, Lua, GOAP, HTN) can turn those intents into `ActionPlans`.

- 🧠 **Lua-First Authoring:** Write entire trees, custom control flows, and planning backends in Lua.
- ⚡ **High Performance Core:** Flat, cache-friendly `DecisionPrograms` with zero string lookups at runtime.
- 🏗️ **Extensible Architecture:** Add new actions, node types, or entire planning algorithms via clean C++ interfaces.

## 🗂️ Project Structure

```text
G.O.A.T/
├── Assets/        # Lua scripts, Editor Icons, Example behaviors
├── Code/          # C++ source, headers, tests, CMake files
├── Docs/          # Obsidian Vault (Project Documentation)
├── Registry/      # Asset Processor settings
└── README.md      # You are here