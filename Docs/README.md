# 🐐 G.O.A.T.

### Gameplay Oriented Agent Toolkit

**Status:** Pre-Alpha  
**Engine:** O3DE  
**Language:** C++ / Lua  

---

## 📖 Overview

**G.O.A.T.** is a genre-neutral NPC AI framework for Open 3D Engine. You write the AI in Lua; C++
provides the infrastructure underneath it.

Instead of locking you into one AI paradigm, the core knows about agents, a blackboard, plans and
actions — and nothing about how decisions get made. A paradigm is a **gem you can delete**:
behaviour trees and hierarchical task networks both ship as separate gems, and both can run in the
same level on different agents.

- 🧠 **Lua-first authoring.** Programs, custom control flow and `delegate` planners, all in Lua.
- 🔌 **Paradigms are removable.** `grep -r 'BehaviorTree\|Htn' Code/Source/Core/` returns nothing.
- 🤝 **They interoperate for free.** A task network writes a blackboard variable; a hundred
  behaviour trees react. One integer write, no wiring, no polling.
- ⚡ **Fast when idle.** ~13 ns per agent for a band tick, 192 bytes per agent, and an agent with
  nothing to do costs one subtraction.
- 🏗️ **Extensible.** Add verbs, words, paradigms or director filters from your own gem.

## 🗂️ Project Structure

```text
G.O.A.T/
├── Assets/                          # Core Lua vocabulary, icons, examples
├── Code/                            # The core gem: C++ source, headers, tests
│   └── Source/Backends/
│       ├── BehaviorTree/            # Gem: the `tree` paradigm
│       └── Htn/                     # Gem: the `htn` paradigm
├── Modules/
│   ├── Navigation/                  # Gem: movement verbs
│   ├── SmartObject/                 # Gem: usable props
│   └── Animation/                   # Gem: animation verbs
├── Docs/                            # Obsidian vault (this documentation)
├── Registry/                        # Asset Processor settings
└── README.md                        # You are here