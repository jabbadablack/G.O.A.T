---
type: theory
status: implemented
tags: [architecture, design, philosophy]
---

# Design Principles

> **Category:** Design Principles  
> **Status:** Implemented  
> **Core Files:** `Code/Source/Core/Frontend/TreeCompiler.cpp`, `Code/Source/Core/Frontend/TreeWalker.cpp`, `Code/Source/Core/Scripting/LuaDispatch.cpp`, `Code/Include/GOAT/Interfaces/IBackend.h`

---

## 💡 Core Concept

G.O.A.T. is not just a Behavior Tree library. It is a **unified decision-making framework** built on six core pillars that shape every architectural decision. These principles ensure the framework remains fast, flexible, and easy to extend without sacrificing performance.

The design philosophy can be summarized as:

> **"Separate what you decide (Lua) from how you execute (C++), and route all decision-making through a single pluggable interface (IBackend)."**

---

## 🗺️ Architecture Overview

```mermaid
graph TD
    subgraph Authoring[Lua Authoring Layer]
        A[Tree DSL] --> B[Behavior Definitions]
        B --> C[Backend Definitions]
        C --> D[Flow Definitions]
    end

    subgraph Core[C++ Core Layer]
        E[LuaDispatch] --> F[LuaTreeBuilder]
        F --> G[TreeCompiler]
        G --> H[DecisionProgram]
        H --> I[TreeWalker]
        I --> J[Intent]
        J --> K[Backend Registry]
    end

    subgraph Execution[Execution Layer]
        K --> L[ActionPlan]
        L --> M[AgentRuntime]
        M --> N[IActionState]
        N --> O[Game World]
    end

    A --> E
    D --> E
```

---

## 🤔 Why This Matters

Without these principles, AI frameworks often devolve into rigid, component-heavy monoliths. For example:

- **Typical BT frameworks** hardcode the tree traversal logic and provide no way to integrate GOAP or HTN alongside BT.
- **Typical HTN frameworks** lack the flexibility of a scripting language and require C++ changes for every new behavior.
- **Typical monolithic AI systems** force all agents to use the same paradigm, making it difficult to have varied NPC behaviors.

G.O.A.T. avoids all of this by enforcing a strict separation between **authoring** (Lua), **compilation** (C++), and **execution** (C++), all tied together by a single `IBackend` abstraction.

---

## ⚖️ The Six Pillars (Detailed)

### 1. Backend as a Planning Abstraction

**Description:**  
All decision-making (Behavior Trees, HTN, GOAP, Utility AI, Director AI) goes through `IBackend`. A tree leaf (`delegate` node) emits an `Intent`, and the backend is responsible for turning that `Intent` into an `ActionPlan`.

**How it works in the code:**

```cpp
// Code/Include/GOAT/Interfaces/IBackend.h
class IBackend
{
public:
    AZ_RTTI(IBackend, IBackendTypeId);

    virtual ~IBackend() = default;

    // Name this backend is registered under and referenced by from Lua.
    virtual AZ::Name GetName() const = 0;

    // Produces a plan for one intent. Returns false when this backend cannot satisfy it.
    virtual bool Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan) = 0;
};
```

**Visual Representation:**

```mermaid
graph LR
    subgraph Tree[Behavior Tree]
        A[delegate node] --> B[Intent]
    end

    subgraph Backends[Backends]
        B --> C[DirectBackend]
        B --> D[LuaBackend]
        B --> E[FutureGOAPBackend]
        B --> F[FutureHTNBackend]
    end

    subgraph Output[Action Plans]
        C --> G[ActionPlan 1]
        D --> H[ActionPlan 2]
        E --> I[ActionPlan 3]
        F --> J[ActionPlan 4]
    end
```

**Concrete examples:**

| Backend | Type | Usage |
| :--- | :--- | :--- |
| `DirectBackend` | C++ | Handles `raw` and `script` leaves. Converts a single action into a one-step plan. |
| `LuaBackend` | Lua | User-defined planning in Lua via `backend "MyGoap" { plan = ... }`. |
| `FutureGoapBackend` | Planned | Could implement full GOAP planning, reusable across all trees. |

**Why this matters:**
- **Flexibility:** A tree can switch from a simple "do this action" to a complex HTN planner without changing the tree structure – just change the `delegate` node.
- **Extensibility:** New AI paradigms can be added by modules without modifying the core engine.
- **Runtime swapping:** The `BackendRegistry` allows backends to be registered/unregistered at runtime.

---

### 2. Lua-First Authoring

**Description:**  
The entire behavior tree DSL is defined in Lua (`GOAT.lua`). Trees, behaviors, flows, and backends are all written in Lua. C++ only compiles and executes the resulting flat structure.

**How it works in the code:**

- `GOAT.lua` defines global functions: `tree`, `behavior`, `flow`, `backend`, and node constructors (`selector`, `sequence`, `wait`, `script`, etc.).
- The `tree` function calls `GOAT.Compile(name, root)` which flattens the node graph into a pre-order record list.
- `LuaDispatch::EmitTree` calls `GOAT_EmitTree`, which pushes node data into a `LuaTreeBuilder`.
- `LuaTreeBuilder` reconstructs a `BehaviorTreeNode` hierarchy.
- `TreeCompiler` compiles that hierarchy into a `DecisionProgram`.

**Visual Representation:**

```mermaid
sequenceDiagram
    participant L as Lua Script (GOAT.lua)
    participant D as LuaDispatch (C++)
    participant B as LuaTreeBuilder (C++)
    participant C as TreeCompiler (C++)
    participant P as DecisionProgram

    L->>D: GOAT_EmitTree("ExampleAgent")
    D->>B: BeginTree("ExampleAgent")
    B->>B: AddNode("selector", 2, 0)
    B->>B: SetStringProperty("key", "target_seen")
    B->>D: Return true (complete)
    D->>C: Compile("ExampleAgent", root)
    C->>P: Flatten nodes & resolve keys
    P-->>C: DecisionProgram object
    C-->>D: Return compiled program
```

**Concrete example (from `ExampleAgent.lua`):**

```lua
-- Author a tree entirely in Lua
behavior "Patrol" {
    tick = function(me, ctx)
        ctx:SetInt("patrol_stop", me.stop)
        return SUCCESS
    end,
}

return tree "ExampleAgent" {
    selector {
        sequence {
            condition "target_seen" { abort = "lower_priority" },
            script "Alert",
            wait(1.0),
        },
        sequence {
            script "Patrol",
            wait(0.5),
        },
    },
}
```

**Why this matters:**
- **Rapid iteration:** Designers can hot-reload scripts without recompiling C++.
- **Full programming power:** Variables, loops, and custom logic can be embedded in behaviors.
- **Extensibility:** New node types, flows, and backends can be defined purely in Lua.
- **Separation of concerns:** C++ handles execution; Lua handles expression.

---

### 3. Schema-Driven Blackboard

**Description:**  
Blackboard variables are declared in `.bbx` assets, merged into a global `BlackboardSchema`, and resolved into typed indices at compile time. No string lookups at runtime.

**How it works in the code:**

- `BlackboardAsset` (.bbx) declares variables with `name`, `type`, `scope`, and `default`.
- `BlackboardSchema::Declare` assigns each variable a typed `BlackboardKey`.
- `TreeCompiler` resolves property names to `BlackboardKey`s at compile time.
- `BlackboardStorage` stores values in typed, contiguous arrays indexed by `BlackboardKey`.

**Visual Representation:**

```mermaid
graph TD
    subgraph Schema[BlackboardSchema]
        A[.bbx Asset] --> B[Declare Variable]
        B --> C[Assign Key]
        C --> D[Typed Slot]
    end

    subgraph Storage[BlackboardStorage]
        D --> E[Bool Array]
        D --> F[Int Array]
        D --> G[Float Array]
        D --> H[Vector3 Array]
        D --> I[EntityId Array]
        D --> J[Name Array]
    end

    subgraph Usage[Tree Execution]
        K[TreeCompiler] -->|Resolves name to key| D
        L[TreeWalker] -->|Reads key| E
        L -->|Reads key| F
        L -->|Reads key| G
    end
```

**Concrete example (from `BlackboardSchema.h`):**

```cpp
class BlackboardSchema final
{
public:
    // Declares one variable and assigns it a slot.
    AZ::Outcome<BlackboardKey, AZStd::string> Declare(
        const AZ::Name& name, BlackboardScope scope, BlackboardType type, AZStd::any defaultValue = {});

    // Resolves a name to its key, or an invalid key when the name is undeclared.
    BlackboardKey Find(const AZ::Name& name) const;
};
```

**Supported types:**

| Type | C++ Type | Lua Type |
| :--- | :--- | :--- |
| Bool | `bool` | `boolean` |
| Int | `AZ::s64` | `number` |
| Float | `float` | `number` |
| Vector3 | `AZ::Vector3` | `Vector3` |
| EntityId | `AZ::EntityId` | `EntityId` |
| Name | `AZ::Name` | `string` |
| Quaternion | `AZ::Quaternion` | `Quaternion` |
| Transform | `AZ::Transform` | `Transform` |
| EntityIdList | `AZStd::vector<AZ::EntityId>` | `table` |

**Why this matters:**
- **No string lookups at runtime** – O(1) array indexing.
- **Type safety** – Compile-time validation catches typos and type mismatches.
- **Cross-scope sharing** – Global, Agent, and Squad scopes allow data to be shared efficiently.
- **Memory efficiency** – Only declared variables are allocated.

---

### 4. Server Authority

**Description:**  
All AI decisions run on the server. Clients only receive replicated blackboard state.

**How it works in the code:**

- `GOATAgentComponent` is only active on the server (`Authority` role).
- `GOATSystemComponent` registers services only on the server.
- Blackboard keys marked as `replicated` are synced to clients via O3DE's replication system.

**Visual Representation:**

```mermaid
graph LR
    subgraph Server[Server - Authority]
        A[GOATAgentComponent] --> B[TreeWalker]
        B --> C[Backend Plan]
        C --> D[Action Execution]
        D --> E[Replicated Blackboard]
    end

    subgraph Client[Client - Visual Only]
        F[Replicated Blackboard] --> G[Visual Smoothing]
        G --> H[Animation / Audio]
    end

    E -->|Delta Compression + Replication| F
```

**Why this matters:**
- **Deterministic behavior** – All AI decisions are consistent across clients.
- **Prevents cheating** – Clients cannot influence AI decisions.
- **Simplifies network code** – Only replicated state needs synchronization.

---

### 5. Performance-Aware Execution

**Description:**  
The framework uses multiple mechanisms to ensure agents run efficiently without wasting CPU or memory.

**Mechanisms:**

| Mechanism | Description |
| :--- | :--- |
| **Tick Bands** | Agents run at different frequencies (Band 0 = most frequent, Band 3 = least frequent). |
| **Interval Services** | Services attached to composites run at fixed intervals, not every frame. |
| **Flat Programs** | Trees compile into contiguous `DecisionNode` arrays with precomputed indices. |
| **Iterative Traversal** | `TreeWalker` uses a loop instead of recursion, avoiding stack overflow on deep trees. |
| **Shared Programs** | Multiple agents running the same tree share an immutable `DecisionProgram`. |
| **Blackboard Indexing** | Values are stored in typed arrays, accessed by index, not string. |

**Visual Representation:**

```mermaid
graph TD
    subgraph Program[Flat DecisionProgram]
        A[Node 0: Selector] --> B[Node 1: Sequence]
        B --> C[Node 2: Condition]
        C --> D[Node 3: Script]
        B --> E[Node 4: Wait]
        A --> F[Node 5: Script]
    end

    subgraph Memory[Memory Layout]
        G[Contiguous Vector<DecisionNode>]
        H[Precomputed Child Indices]
        I[Typed Blackboard Arrays]
    end

    Program --> Memory
```

**Concrete example (from `TreeCompiler.cpp`):**

```cpp
// Flattening the tree into a contiguous array
const NodeIndex index = aznumeric_cast<NodeIndex>(program.m_nodes.size());
program.m_nodes.emplace_back();
{
    DecisionNode& node = program.m_nodes[index];
    node.m_op = descriptor->m_op;
    node.m_parent = parent;
    node.m_childCount = aznumeric_cast<AZ::u16>(authored.m_children.size());
}
```

**Why this matters:**
- **Scalability** – Supports thousands of agents without frame spikes.
- **Cache locality** – Flat arrays are cache-friendly.
- **Predictable performance** – No hidden allocations or recursion.

---

### 6. Behavior-Driven Data

**Description:**  
Behaviors declare which blackboard keys they need. The system dynamically provisions only those keys, saving memory and enforcing correctness.

**How it works in the code:**

- `TreeCompiler` collects `observedKeys` for conditions with `abort` modes.
- `BlackboardStorage::EnsureCapacity` resizes arrays only when new keys are added.
- `LuaPlanBuilder` validates that steps reference existing blackboard keys.

**Visual Representation:**

```mermaid
graph TD
    subgraph Authoring[Authoring]
        A[Tree Definition] --> B[References Blackboard Keys]
    end

    subgraph Compilation[Compilation]
        B --> C[TreeCompiler]
        C --> D[Collects Observed Keys]
        D --> E[Validates References]
    end

    subgraph Storage[Storage]
        E --> F[BlackboardSchema]
        F --> G[Ensures Capacity]
        G --> H[Allocates Only Needed Slots]
    end
```

**Why this matters:**
- **Memory efficiency** – No wasted storage for unused variables.
- **Correctness** – Referencing an undeclared variable is caught at compile time, not runtime.
- **Designer-friendly** – Clear error messages help designers find issues.

---

## 🧩 Impact on the Codebase

### Lua Layer

- `GOAT.lua` defines the entire vocabulary (`tree`, `behavior`, `flow`, `backend`).
- Users write trees in pure Lua, customizing control flow (`flow`) and planning (`backend`) without touching C++.
- The `ctx` object passed to behaviors gives type-safe blackboard access (`SetBool`, `GetInt`, etc.).

### C++ Core

- `TreeCompiler` validates authored trees and flattens them into `DecisionProgram`s.
- `TreeWalker` executes these flat programs iteratively, emitting `Intents`.
- `DirectBackend` and `LuaBackend` interpret these `Intents` and produce `ActionPlan`s.
- `AgentRuntime` processes `ActionPlan`s, executing each `ActionRequest` through registered `IActionState`s.

### Extensibility

- Modules register new `IActionState`s (e.g., `WaitAction`, `RunScriptAction`) via `IAgentSystem::RegisterAction`.
- New planning algorithms (HTN, GOAP) are added by implementing `IBackend` and registering it via `IAgentSystem::RegisterBackend`.
- New node types are added via `NodeTypeRegistry`.

---

## 🗺️ Future Evolution

These principles are designed to accommodate planned modules like [[Navigation Library]] and [[Perception Module]].

### Navigation Library

- **How it fits:** Will be implemented as a library of `IActionState`s (e.g., `MoveTo`, `Wander`), not a component.
- **Registration:** Each navigation action will be registered via `IAgentSystem::RegisterAction`.
- **Blackboard Integration:** Target positions will be stored in `Vector3` blackboard keys.
- **Why not a component?** Navigation is a service, not a state. Components bring replication and lifecycle overhead; a library keeps it lightweight and reusable.

### Perception Module

- **How it fits:** Will likely be implemented as Lua `service` nodes that poll world state and write to the blackboard.
- **Example service:**
  ```lua
  behavior "Sense" {
      tick = function(me, ctx)
          ctx:SetBool("target_seen", ctx:GetInt("patrol_stop") % 4 == 0)
      end,
  }
  ```
- **Why a service?** Services run at fixed intervals, reducing CPU cost compared to per-frame checks. They respect the "Performance-Aware Execution" and "Behavior-Driven Data" principles.

---

## 🔗 Related Concepts

- [[Backend Abstraction Theory]]
- [[Performance Model]]
- [[Extensibility Model]]
- [[Blackboard System]]

---

*Last updated: 2026-08-26*