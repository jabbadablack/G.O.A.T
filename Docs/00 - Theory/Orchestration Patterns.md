---
type: theory
status: concept
tags: [theory, director, architecture]
---

# Orchestration Patterns

> **Category:** Architectural Pattern  
> **Status:** Implemented via `IBackend` and Global Agents  

---

## 💡 Core Concept

**Orchestration Patterns** are architectural approaches for controlling groups of agents from a central authority. In G.O.A.T., this is achieved using the **Backend Abstraction** combined with **Global and Squad Blackboard Scopes**. Instead of adding a complex new system, orchestration leverages the existing planning pipeline.

---

## 🤔 Why This Matters

Games often need a "brain" that coordinates many NPCs:

- **Wave-based combat:** The game decides when to spawn enemies and how many.
- **Difficulty scaling:** The game modifies enemy health or speed based on player progress.
- **Environmental events:** The game triggers earthquakes, ambushes, or patrol changes.

Without a clean pattern, this logic often becomes scattered across many components. Orchestration Patterns provide a **single, unified way** to handle global decisions.

---

## 🗺️ Visual Overview

```mermaid
graph TD
    subgraph Director[Director Agent]
        A[Global Entity] --> B[GOATAgentComponent]
        B --> C[Band 3 - 1000ms]
        C --> D[Global Tree]
        D --> E[delegate Director]
        E --> F[Director Backend]
        F --> G[ActionPlan]
    end

    subgraph Blackboard[Blackboard Scopes]
        G --> H[Global Scope]
        H --> I[wave_number]
        H --> J[game_state]
        G --> K[Squad Scope]
        K --> L[Squad Leader]
        K --> M[Squad Target]
    end

    subgraph Agents[Individual Agents]
        I --> N[Agent 1]
        J --> O[Agent 2]
        L --> P[Agent 3]
        M --> Q[Agent 4]
    end
```

---

## ⚖️ The Three Orchestration Styles

### 1. Global Agent (Entity-Based)

The most direct approach. A single entity runs a tree that delegates to a Director backend.

```mermaid
graph TD
    A[Global Entity] --> B[Band 3 Tick]
    B --> C[Global Tree]
    C --> D[delegate Director]
    D --> E[Plan]
```

**Pros:** Simple, uses existing components.  
**Cons:** Requires an actual entity in the scene.

---

### 2. Backend-Only (Service-Based)

No dedicated entity. Instead, a **service** runs on a high-priority agent and writes to the Global Blackboard.

```lua
service "Director" { interval = 1.0 }
```

The service polls game state and writes to Global variables.

**Pros:** No extra entity.  
**Cons:** Tied to the life of that specific agent.

---

### 3. Hybrid (Global Agent + Service)

The most flexible. A Global Agent uses a service to monitor state and a `delegate` to execute directives.

```lua
tree "DirectorTree" {
    selector {
        service "CheckProgress" { interval = 1.0 },
        sequence {
            condition "wave_active" { abort = "lower_priority" },
            delegate "Director" { goal = "SpawnWave" },
        },
    }
}
```

**Pros:** Combines reactive monitoring with planning.  
**Cons:** Slightly more complex to author.

---

## 🧩 How Orchestration Fits G.O.A.T.'s Principles

| Principle | How Orchestration Uses It |
| :--- | :--- |
| **Backend Abstraction** | The Director is just a backend (`IBackend`). |
| **Lua-First Authoring** | Global trees, services, and backend plans are all written in Lua. |
| **Schema-Driven Blackboard** | Global and Squad scopes allow cross-agent data sharing. |
| **Performance-Aware** | Band 3 for global agents minimizes CPU cost. |
| **Behavior-Driven Data** | Agents react to Global keys via conditions and aborts. |
| **Event-Driven Guards** | `AgentObserver` wakes agents only when watched keys change. |

---

## 🧩 Example: Wave Spawning

```lua
-- Global tree
return tree "DirectorTree" {
    sequence {
        condition "wave_started" { abort = "lower_priority" },
        delegate "Director" { goal = "SpawnWave" },
        wait(2.0),
    },
}

-- Director backend
backend "Director" {
    plan = function(me, ctx, goal)
        local wave = ctx:GetInt("wave_number") or 1
        return {
            { action = "script", behavior = "SpawnEnemies", tag = "wave_" .. wave },
            { action = "wait", seconds = 10.0 },
        }
    end,
}
```

---

## 🧩 Example: Difficulty Scaling

```lua
-- Global tree
return tree "DifficultyTree" {
    sequence {
        condition "player_progress" { abort = "lower_priority" },
        delegate "Director" { goal = "ScaleDifficulty" },
        wait(5.0),
    },
}

-- Director backend
backend "Director" {
    plan = function(me, ctx, goal)
        local progress = ctx:GetInt("player_progress") or 1
        local difficulty = math.floor(progress / 10)
        return {
            { action = "script", behavior = "SetDifficulty", tag = "difficulty_" .. difficulty },
        }
    end,
}
```

---

## 🧩 Example: Squad Coordination

```lua
-- Global tree
return tree "SquadTree" {
    sequence {
        condition "squad_target" { abort = "lower_priority" },
        delegate "Director" { goal = "CoordinateSquad" },
        wait(1.0),
    },
}

-- Director backend
backend "Director" {
    plan = function(me, ctx, goal)
        if goal == "CoordinateSquad" then
            local squad = ctx:GetName("active_squad")
            return {
                { action = "script", behavior = "SetSquadFormation", tag = squad },
                { action = "wait", seconds = 2.0 },
            }
        end
        return nil
    end,
}
```

---

## ⚖️ Trade-offs

| ✅ Advantage | ⚠️ Disadvantage |
| :--- | :--- |
| Centralized control over game flow | Single point of failure if not designed carefully |
| Minimal CPU cost (runs at Band 3) | Requires careful management of Global Blackboard state |
| Uses existing Backend abstraction | Need to design a tree that handles multiple goals |
| No new C++ components needed | Debugging can be harder with cross-scope effects |
| Leverages `AgentRegistry` scheduling | Must ensure Director entity is always active |

---

## 🧩 Impact on the Codebase

### Lua Layer

- Global trees are authored in Lua like any other tree.
- Services with `interval` run at fixed rates.
- Backends are defined in Lua via `backend "Name" { plan = ... }`.

### C++ Core

- `AgentRegistry` schedules the Director entity at Band 3.
- `BackendRegistry` routes `delegate` intents to the Director backend.
- `BlackboardSystem` provides Global and Squad storage.
- `AgentObserver` wakes the Director only when watched Global keys change.

### Extensibility

- New orchestration styles can be added without touching core.
- Directors can be swapped at runtime by rebinding backend names.
- Squad coordination uses the same `SquadRegistry` as individual agents.

---

## 🗺️ Future Evolution

- **Director AI** can be extended to support multiple simultaneous goals.
- **Squad Registry** can be used for dynamic grouping and formation control.
- **Navigation Library** will allow Directors to move entities (e.g., spawn points, patrol paths).

---

## 🔗 Related Concepts

- [[Director AI]]
- [[Backend Abstraction Theory]]
- [[Design Principles]]
- [[Performance Model]]
- [[Extensibility Model]]

---

*Last updated: 2026-08-26*