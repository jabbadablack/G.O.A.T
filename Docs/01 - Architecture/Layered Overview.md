---
type: architecture
status: implemented
tags: [architecture, core]
---

# Layered Overview

> **Category:** Architecture Overview  
> **Status:** Implemented  
> **Core Files:** `Code/Source/Clients/GOATAgentComponent.cpp`, `Code/Source/Core/Application/GOATSystemComponent.cpp`, `Code/Source/Core/Scripting/LuaDispatch.cpp`, `Code/Source/Core/Frontend/TreeCompiler.cpp`, `Code/Source/Core/Frontend/TreeWalker.cpp`

---

## 💡 Core Concept

G.O.A.T. is organized into **three distinct layers**, each with a clear and rigid responsibility. This separation ensures that the framework remains modular, testable, and easy to extend.

| Layer | Core Responsibility | Primary Files |
| :--- | :--- | :--- |
| **Lua Authoring Layer** | Define trees, behaviors, flows, and backends | `GOAT.lua`, `ExampleAgent.lua`, `ExampleAdvanced.lua` |
| **C++ Core Layer** | Compile, execute, plan, and manage agents | `LuaDispatch`, `TreeCompiler`, `TreeWalker`, `AgentRuntime`, `BlackboardSystem` |
| **Runtime Component Layer** | Bridge O3DE entities to the AI system | `GOATAgentComponent`, `GOATSystemComponent` |

---

## 🗺️ Visual Overview (Main Architecture)

```mermaid
graph TD
    subgraph Lua[Lua Authoring Layer]
        A[Tree Authoring] --> B[Behavior Definitions]
        B --> C[Flow Definitions]
        C --> D[Backend Definitions]
    end

    subgraph Core[C++ Core Layer]
        E[LuaDispatch] --> F[LuaTreeBuilder]
        F --> G[TreeCompiler]
        G --> H[DecisionProgram]
        H --> I[TreeWalker]
        I --> J[Intent]
        J --> K[BackendRegistry]
        K --> L[ActionPlan]
        L --> M[AgentRuntime]
        M --> N[IActionState]
    end

    subgraph Runtime[Runtime Component Layer]
        O[GOATAgentComponent] --> E
        P[GOATSystemComponent] --> E
        O --> Q[Entity]
        P --> R[Registries]
    end

    N --> Q
    R --> O
```

---

## 🗺️ Visual Overview (Data Flow Bridge)

This diagram shows the exact communication path between the three layers.

```mermaid
graph LR
    L[Lua Script] -->|GOAT_EmitTree| D[LuaDispatch]
    D -->|BeginTree / AddNode| B[LuaTreeBuilder]
    B -->|BehaviorTreeNode| C[TreeCompiler]
    C -->|DecisionProgram| W[TreeWalker]
    W -->|Intent| K[BackendRegistry]
    K -->|ActionPlan| R[AgentRuntime]
    R -->|IActionState| A[Game World]
    A -->|Blackboard Updates| W
```

---

## 🧩 Layer 1: Lua Authoring Layer

### Purpose
Provide a designer-friendly DSL for authoring AI behavior without touching C++. This layer is the **source of truth** for what an agent does.

### Key Components
- **`GOAT.lua`**: Defines the global vocabulary (`tree`, `behavior`, `flow`, `backend`) and node constructors (`selector`, `sequence`, `wait`, `script`).
- **User Scripts (e.g., `ExampleAgent.lua`)**: The actual game-specific logic where designers define their custom trees and behaviors.

### What happens here
1. The `tree` function compiles the node graph into a flat, pre-order record list.
2. The compiled record is stored in `GOAT._trees[name]`.
3. `GOAT_Compile` flattens the nested node syntax into a plain array that C++ can consume.

### Detailed Code Flow
```lua
-- Author a tree
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

---

## 🧩 Layer 2: C++ Core Layer

### Purpose
Process and execute the authored trees. This layer handles performance-critical operations, validation, and planning.

### Subsystem Breakdown

```mermaid
graph LR
    Bridge[Bridge] --> Compiler[Compiler]
    Compiler --> Executor[Executor]
    Executor --> Planning[Planning]
    Planning --> Runtime[Runtime]
    Runtime --> Data[Data]
```

| Subsystem | Responsibility | Key Files |
| :--- | :--- | :--- |
| **Bridge** | Calls into the Lua VM (`GOAT_EmitTree`, `GOAT_Dispatch`) | `LuaDispatch.cpp`, `LuaTreeBuilder.cpp` |
| **Compiler** | Validates nodes, resolves blackboard keys, flattens the tree | `TreeCompiler.cpp` |
| **Executor** | Iteratively walks the `DecisionProgram`, producing `Intent`s | `TreeWalker.cpp`, `DecisionCursor.h` |
| **Planning** | Turns `Intent`s into executable `ActionPlan`s | `BackendRegistry.cpp`, `DirectBackend.cpp`, `LuaBackend.cpp` |
| **Runtime** | Executes `ActionPlan`s, manages tick bands and active actions | `AgentRuntime.cpp`, `AgentRegistry.cpp` |
| **Data** | Stores typed blackboard variables across scopes | `BlackboardSystem.cpp`, `BlackboardStorage.cpp` |

### The Compilation Pipeline
1. **LuaDispatch** calls `GOAT_EmitTree`.
2. **LuaTreeBuilder** reconstructs a `BehaviorTreeNode` hierarchy.
3. **TreeCompiler** validates the node types and checks against the `BlackboardSchema`.
4. **TreeCompiler** emits a flat `DecisionProgram` (a contiguous array of `DecisionNode`s).
5. **TreeWalker** executes this program iteratively, maintaining state in a `DecisionCursor`.

```cpp
// Code/Source/Core/Scripting/LuaDispatch.cpp
AZ::Outcome<AZStd::shared_ptr<const BehaviorTreeNode>, AZStd::string> LuaDispatch::EmitTree(const AZ::Name& treeName)
{
    AZ::ScriptDataContext call;
    if (!m_scriptContext->Call("GOAT_EmitTree", call))
    {
        return AZ::Failure(AZStd::string("The GOAT Lua vocabulary is not loaded"));
    }

    call.PushArg(AZStd::string(treeName.GetStringView()));
    call.PushArg(m_builder);

    if (!call.CallExecute())
    {
        return AZ::Failure(AZStd::string::format("Emitting tree '%s' raised a Lua error", treeName.GetCStr()));
    }

    if (!m_builder.IsComplete())
    {
        return AZ::Failure(AZStd::string::format(
            "Tree '%s' could not be assembled: %s", treeName.GetCStr(), m_builder.GetError().c_str()));
    }

    return AZ::Success(AZStd::shared_ptr<const BehaviorTreeNode>(aznew BehaviorTreeNode(m_builder.GetRoot())));
}
```

---

## 🧩 Layer 3: Runtime Component Layer

### Purpose
Bridge O3DE entities to the AI system. This layer handles the lifecycle of an agent, loading assets, and exposing the `IAgentSystem` interface to the rest of the engine.

### Key Components
- **`GOATAgentComponent`**: Attached to an NPC entity. Handles `Activate()` and `Deactivate()`.
- **`GOATSystemComponent`**: A singleton "God object" that owns all services. Implements `IAgentSystem`.

### Agent Lifecycle Diagram

```mermaid
graph LR
    A[Entity Activated] --> B[GOATAgentComponent Activate]
    B --> C[Load Blackboard Assets]
    C --> D[Load Lua Scripts]
    D --> E[Compile Tree]
    E --> F[Register Agent]
    F --> G[AgentRegistry]
    G --> H[AgentRuntime Begins Tick]
```

### Detailed Lifecycle
1. **`GOATAgentComponent::Activate()`** is called by O3DE.
2. It loads `.bbx` assets to declare blackboard variables.
3. It loads Lua scripts to register behaviors and trees.
4. It calls `CompileTree()` (which triggers `TreeCompiler`).
5. It calls `RegisterAgent()` on `IAgentSystem`.
6. The `AgentRegistry` creates an `AgentId` and registers the `DecisionProgram` for the agent.

```cpp
// Code/Source/Clients/GOATAgentComponent.cpp
void GOATAgentComponent::Activate()
{
    IAgentSystem* agents = AgentSystemInterface::Get();
    if (agents == nullptr)
    {
        AZ_Warning("GOAT", false, "The GOAT agent system is not available");
        return;
    }

    for (auto& blackboard : m_blackboards) { agents->LoadBlackboard(*blackboard.Get()); }
    for (auto& script : m_scripts) { agents->LoadScript(script); }

    if (auto compiled = agents->CompileTree(AZ::Name(m_treeName)); !compiled.IsSuccess())
    {
        AZ_Warning("GOAT", false, "%s", compiled.GetError().c_str());
        return;
    }

    m_agent = agents->RegisterAgent(GetEntityId(), AZ::Name(m_treeName), static_cast<size_t>(m_band));
}
```

---

## 🧩 Layer Interaction: The Bridge

The **C++ Core Layer** and **Lua Authoring Layer** communicate through a strict, push-based bridge.

```mermaid
graph LR
    subgraph Lua[Lua Layer]
        S[Script]
    end
    subgraph Cpp[C++ Layer]
        D[LuaDispatch]
        B[LuaTreeBuilder]
        C[TreeCompiler]
    end
    S -->|Call GOAT_EmitTree| D
    D -->|PushNode| B
    B -->|Rebuild Hierarchy| C
    C -->|Validate| P[DecisionProgram]
```

| Step | Caller | Callee | Data Passed |
| :--- | :--- | :--- | :--- |
| 1 | Lua Script | `LuaDispatch` | Function call: `GOAT_EmitTree` |
| 2 | `LuaDispatch` | `LuaTreeBuilder` | `BeginTree`, `AddNode`, `SetProperty` |
| 3 | `LuaDispatch` | `LuaTreeBuilder` | `EndTree` (completes assembly) |
| 4 | `GOATSystemComponent` | `TreeCompiler` | `BehaviorTreeNode` (root) |
| 5 | `TreeCompiler` | `DecisionProgram` | `Compile` (flattened array) |

---

## ⚖️ Trade-offs

| ✅ Advantage | ⚠️ Disadvantage |
| :--- | :--- |
| Clear separation of concerns | Requires understanding all three layers to debug deep issues |
| Lua makes authoring fast and safe (hot-reload) | Lua errors can be harder to trace without proper stack unwinding |
| C++ core is performant (no string lookups, flat arrays) | Compile-time validation adds complexity for new node types |
| Modular and extensible (Backends, Actions) | More files to navigate when starting out |

---

## 🧩 Performance Considerations per Layer

- **Lua Layer**: Lightweight; only runs on script load and `GOAT_Dispatch` calls.
- **C++ Core**: Runs every tick (depending on Band). Uses `DecisionCursor` and `HandleTable` to avoid heap allocations during runtime.
- **Runtime Layer**: Very lightweight; `GOATAgentComponent` is a thin shell that just passes data.

---

## 🔗 Related Notes

- [[Data Flow]]
- [[Blackboard System]]
- [[GOATSystemComponent]]
- [[GOATAgentComponent]]
- [[Extensibility Model]]

---

*Last updated: 2026-08-26*