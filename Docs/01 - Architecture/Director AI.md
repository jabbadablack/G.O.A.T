---
type: architecture
status: planned
tags: [architecture, director, ai]
---

# Director AI

> **Category:** Architecture Pattern  
> **Status:** Implemented via Backend Abstraction (no specific C++ component)  
> **Core Files:** `Code/Source/Clients/GOATAgentComponent.cpp`, `Code/Source/Core/Application/GOATSystemComponent.cpp`

---

## 💡 Core Concept

**Director AI** is a global orchestrator that controls high-level game flow—spawning waves, adjusting difficulty, or reacting to player progress. It is **not a separate component** or system. Instead, it is a **specific usage pattern** of G.O.A.T.'s `IBackend` abstraction combined with a "Global Agent" entity.

The Director operates at the **Global** and **Squad** blackboard scopes, making decisions that affect many agents at once.

---

## 🗺️ Visual Overview

```mermaid
graph TD
    A[Global Director Entity] --> B[GOATAgentComponent]
    B --> C[Global Tree]
    C --> D[delegate node]
    D --> E[Director Backend]
    E --> F[ActionPlan]
    F --> G[Global Blackboard Writes]
    G --> H[Squad Blackboard Writes]
    G --> I[Agent Blackboard Reads]
```

---

## 🧩 How it Fits into the Architecture

### Layered Placement

| Layer | Director's Role |
| :--- | :--- |
| **Lua Authoring** | Define a global tree with `delegate "Director" { goal = "SpawnWave" }`. |
| **C++ Core** | `TreeWalker` executes the global tree; `BackendRegistry` routes to a `DirectorBackend`. |
| **Runtime Component** | A single `GOATAgentComponent` on a "Director" entity, running at the lowest tick frequency (`Band = 3`). |

---

## 🧩 Setting Up a Director Agent

1. **Create an Entity** named "Director" in the level.
2. **Add `GOATAgentComponent`** to it.
3. **Set `Band` to `3`** (runs every 8th frame, saving CPU).
4. **Set `TreeName`** to a global tree (e.g., `"DirectorTree"`).
5. **Load a `.bbx` asset** that declares `Global` scope variables (e.g., `game_state`, `wave_number`).

---

## 🧩 The Director Backend

The Director's logic lives in a backend (C++ or Lua). It receives a goal from a `delegate` node and returns a plan of actions.

### Example (Lua):

```lua
backend "Director" {
    plan = function(me, ctx, goal)
        if goal == "SpawnWave" then
            return {
                { action = "script", behavior = "SpawnEnemies" },
                { action = "wait", seconds = 5.0 },
            }
        end
        return nil
    end,
}
```

### Example (Global Tree):

```lua
return tree "DirectorTree" {
    sequence {
        condition "wave_started" { abort = "lower_priority" },
        delegate "Director" { goal = "SpawnWave" },
        wait(2.0),
    },
}
```

---

## 🧩 Data Flow: Director to Agents

```mermaid
flowchart LR
    A[Director Entity Tick] --> B[TreeWalker]
    B --> C[Director Backend]
    C --> D[Plan Execution]
    D --> E[Write Global Keys]
    E --> F[Write Squad Keys]
    F --> G[Agents Observe Keys]
    G --> H[Agents Preempt / React]
```

---

## ⚖️ Trade-offs

| ✅ Advantage | ⚠️ Disadvantage |
| :--- | :--- |
| Centralized control over game flow | Single point of failure if not designed carefully |
| Minimal CPU cost (runs at Band 3) | Requires careful management of Global Blackboard state |
| Uses existing Backend abstraction | Need to design a tree that handles multiple goals |
| No new C++ components needed | Debugging can be harder with cross-scope effects |

---

## 🔗 Related Notes

- [[Orchestration Patterns]]
- [[Backend Abstraction Theory]]
- [[Layered Overview]]
- [[Blackboard System]]
- [[Extensibility Model]]

---

*Last updated: 2026-08-26*