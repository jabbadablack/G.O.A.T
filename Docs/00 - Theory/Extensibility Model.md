---
type: theory
status: implemented
tags: [architecture, design, extensibility]
---

# Extensibility Model

> **Category:** Architectural Pattern  
> **Status:** Implemented  
> **Core Files:** `Code/Source/Clients/GOATAgentComponent.cpp`, `Code/Source/Core/Application/ActionStateRegistry.cpp`, `Code/Source/Core/Application/BackendRegistry.cpp`, `Code/Source/Core/Application/NodeTypeRegistry.cpp`, `Code/Include/GOAT/Interfaces/IAgentSystem.h`

---

## 💡 Core Concept

G.O.A.T. is designed so that **you never need to modify the core engine** to add new behavior. Instead, you extend it through three primary hook points:

1. **Action States (`IActionState`)** – Add new verbs (e.g., `MoveTo`, `Attack`, `PlayAnimation`).
2. **Backends (`IBackend`)** – Add new planning paradigms (e.g., GOAP, HTN, Director AI).
3. **Node Types (`NodeTypeRegistry`)** – Add new tree node types that the Lua DSL can use.

This is achieved through the `IAgentSystem` interface, which acts as a **registry façade** for the entire framework.

---

## 🗺️ Visual Overview

```mermaid
graph TD
    subgraph Core[Core Engine]
        A[GOATSystemComponent] --> B[IAgentSystem]
        B --> C[ActionStateRegistry]
        B --> D[BackendRegistry]
        B --> E[NodeTypeRegistry]
    end

    subgraph Extensions[Extension Points]
        C --> F[IActionState Implementations]
        D --> G[IBackend Implementations]
        E --> H[NodeType Descriptors]
    end

    subgraph Modules[Modules & Gems]
        F --> I[Navigation Module]
        F --> J[Animation Module]
        G --> K[GOAP Backend]
        G --> L[Director AI Backend]
        H --> M[Custom Node Types]
    end

    subgraph Authoring[Lua Authoring]
        I --> N[script MoveTo]
        K --> O[delegate MyGoap]
        M --> P[selector / sequence]
    end
```

---

## 🤔 Why This Matters

### The Problem

Most game AI frameworks are **closed** – you have to modify the core source code to add new node types, actions, or planning algorithms. This creates:

- **Fork risk:** Every project modifies core code, making upgrades impossible.
- **Integration pain:** New features require rebuilding the entire engine.
- **Designer frustration:** AI designers are limited to what the core provides.

### The G.O.A.T. Solution

G.O.A.T. treats extensibility as a **first-class concern**. The core engine provides only the **pipeline** (compile, walk, plan, execute). Everything else is pluggable through clean interfaces. This means:

- **Modules add behavior** without touching core.
- **Designers use new verbs** directly in Lua trees.
- **New planning algorithms** are swapped in without changing tree structure.

---

## 🧩 The Three Extension Points

### 1. Action States (`IActionState`)

**Description:**  
Action states are the **atomic operations** an agent can perform. They are registered by name and invoked by `ActionPlan`s.

**Interface (from `IActionState.h`):**

```cpp
class IActionState
{
public:
    AZ_RTTI(IActionState, IActionStateTypeId);

    virtual ~IActionState() = default;

    // Name this verb is registered under and referenced by from Lua.
    virtual AZ::Name GetName() const = 0;

    // Begins the action for one agent.
    virtual void Begin(const ActionContext& context) = 0;

    // Advances the action. The agent stays in this state while it returns Running.
    virtual ActionResult Step(const ActionContext& context, float deltaTime) = 0;

    // Ends the action, whether it completed or was aborted.
    virtual void End(const ActionContext& context) = 0;
};
```

**How to register:**

```cpp
// Code/Source/Core/Application/GOATSystemComponent.cpp
m_actions->RegisterAt(CoreActions::Wait, AZStd::make_unique<WaitAction>());
m_actions->RegisterAt(CoreActions::RunScript, AZStd::make_unique<RunScriptAction>(*m_dispatch, *m_scriptContext));
```

For custom actions (from modules):

```cpp
// MyModuleSystemComponent.cpp
if (auto* agentSystem = GOAT::AgentSystemInterface::Get())
{
    agentSystem->RegisterAction(AZStd::make_unique<MoveToAction>());
}
```

**Visual Representation:**

```mermaid
graph LR
    subgraph Registry[ActionStateRegistry]
        A[RegisterAt Wait] --> B[WaitAction]
        A --> C[RegisterAt RunScript] --> D[RunScriptAction]
        E[Register MoveTo] --> F[MoveToAction]
        E --> G[Register PlayAnim] --> H[PlayAnimationAction]
    end

    subgraph Usage[Usage in Trees]
        B --> I[raw wait or script Wait]
        D --> J[script RunScript]
        F --> K[raw MoveTo or script MoveTo]
        H --> L[script PlayAnim]
    end
```

**How to create a new one:**

```cpp
class MoveToAction final : public IActionState
{
public:
    AZ_CLASS_ALLOCATOR(MoveToAction, AZ::SystemAllocator);

    AZ::Name GetName() const override { return AZ_NAME_LITERAL("MoveTo"); }

    void Begin(const ActionContext& context) override
    {
        // Store state in context.m_scratch
    }

    ActionResult Step(const ActionContext& context, float deltaTime) override
    {
        // Update movement
        return ActionResult::Running;
    }

    void End(const ActionContext& context) override
    {
        // Cancel movement
    }
};
```

---

### 2. Backends (`IBackend`)

**Description:**  
Backends are **planning algorithms** that turn `Intent`s from tree leaves into `ActionPlan`s. This is how you add GOAP, HTN, Utility AI, or Director AI.

**Interface (from `IBackend.h`):**

```cpp
class IBackend
{
public:
    AZ_RTTI(IBackend, IBackendTypeId);

    virtual ~IBackend() = default;

    // Name this backend is registered under and referenced by from Lua.
    virtual AZ::Name GetName() const = 0;

    // Produces a plan for one intent. Returns false when this backend cannot satisfy it.
    virtual bool Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan) = 0;

    // Reports the conditions that invalidate the plan while it runs.
    virtual void CollectGuards(
        [[maybe_unused]] const PlanContext& context,
        [[maybe_unused]] const ActionPlan& plan,
        [[maybe_unused]] GuardList& outGuards) const { }

    // Releases any per agent state held for this agent.
    virtual void Release([[maybe_unused]] const PlanContext& context) { }
};
```

**How to register:**

```cpp
// Code/Source/Core/Application/GOATSystemComponent.cpp
auto direct = AZStd::make_unique<DirectBackend>();
m_directBackend = AZStd::move(direct);
```

```cpp
// Code/Include/GOAT/Interfaces/IAgentSystem.h
virtual bool RegisterBackend(AZStd::unique_ptr<IBackend> backend) = 0;
```

**Visual Representation:**

```mermaid
graph TD
    subgraph Registry[BackendRegistry]
        A[Register direct] --> B[DirectBackend]
        C[Register Errand Lua] --> D[LuaBackend]
        E[Register Goap Future] --> F[GoapBackend]
        G[Register Director Future] --> H[DirectorBackend]
    end

    subgraph Usage[Usage in Trees]
        B --> I[raw wait]
        D --> J[delegate Errand goal Deliver]
        F --> K[delegate Goap goal DefeatEnemy]
        H --> L[delegate Director goal SpawnWave]
    end
```

**How to create a new one (C++):**

```cpp
class GoapBackend final : public IBackend
{
public:
    AZ_CLASS_ALLOCATOR(GoapBackend, AZ::SystemAllocator);

    AZ::Name GetName() const override { return AZ_NAME_LITERAL("Goap"); }

    bool Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan) override
    {
        // Run GOAP algorithm, populate outPlan
        return true;
    }
};
```

**How to create a new one (Lua):**

```lua
-- Example from ExampleAdvanced.lua
backend "Errand" {
    plan = function(me, ctx, goal)
        if goal == "Rest" then
            return { { action = "wait", seconds = 2.0 } }
        end
        return {
            { action = "script", behavior = "Announce" },
            { action = "wait", seconds = 0.5 },
        }
    end,
}
```

---

### 3. Node Types (`NodeTypeRegistry`)

**Description:**  
Node types define the **vocabulary** available in trees. They are registered with a name, a kind (`NodeKind`), an operation (`NodeOp`), and a set of accepted parameters.

**Interface (from `NodeTypeRegistry.h`):**

```cpp
class NodeTypeRegistry final
{
public:
    NodeTypeRegistry(); // Seeds built-ins

    bool Register(NodeTypeDescriptor descriptor);
    void Unregister(const AZ::Name& name);
    const NodeTypeDescriptor* Find(const AZ::Name& name) const;
    AZStd::vector<const NodeTypeDescriptor*> GetAll() const;
};
```

**NodeTypeDescriptor (from `NodeType.h`):**

```cpp
struct NodeTypeDescriptor
{
    AZ::Name m_name;
    NodeKind m_kind;
    NodeOp m_op;
    AZStd::string m_category;
    AZStd::string m_description;
    AZStd::vector<NodeParameter> m_parameters;
};
```

**Visual Representation:**

```mermaid
graph TD
    subgraph Registry[NodeTypeRegistry]
        A[Register selector] --> B[NodeKind Composite]
        A --> C[Register script] --> D[NodeKind Leaf]
        A --> E[Register condition] --> F[NodeKind Decorator]
        G[Register CustomNode Future] --> H[NodeKind Decorator]
    end

    subgraph Authoring[Usage in Lua]
        B --> I[selector ...]
        D --> J[script Patrol]
        F --> K[condition TargetVisible]
        H --> L[custom MyDecorator ...]
    end
```

**How to create a new one (C++):**

```cpp
NodeTypeDescriptor descriptor;
descriptor.m_name = AZ_NAME_LITERAL("MyCustomNode");
descriptor.m_kind = NodeKind::Decorator;
descriptor.m_op = NodeOp::LuaDecorator;
descriptor.m_parameters.push_back({ AZ_NAME_LITERAL("behavior"), BlackboardType::Name, false, true });
m_nodeTypes->Register(descriptor);
```

---

## 🧩 The `IAgentSystem` Façade

All extension points go through `IAgentSystem`, which is registered by `GOATSystemComponent`.

```cpp
// Code/Include/GOAT/Interfaces/IAgentSystem.h
class IAgentSystem
{
public:
    AZ_RTTI(IAgentSystem, IAgentSystemTypeId);

    virtual ~IAgentSystem() = default;

    // Runs a Lua script, registering whatever behaviours, backends and trees it declares.
    virtual bool LoadScript(const AZ::Data::Asset<AZ::ScriptAsset>& asset) = 0;

    // Declares the variables a blackboard asset holds. Duplicate names fail.
    virtual AZ::Outcome<void, AZStd::string> LoadBlackboard(const BlackboardAsset& asset) = 0;

    // Compiles a declared tree so agents can run it.
    virtual AZ::Outcome<void, AZStd::string> CompileTree(const AZ::Name& treeName) = 0;

    // Registers an entity as an agent running a compiled tree.
    virtual AgentId RegisterAgent(AZ::EntityId entity, const AZ::Name& treeName, size_t band) = 0;

    // Removes an agent and everything held for it.
    virtual void UnregisterAgent(AgentId agent) = 0;

    // Puts an agent in a named squad, creating that squad on the first join.
    virtual void JoinSquad(AgentId agent, const AZ::Name& squad) = 0;

    // Installs a backend. Removing one is what makes backends decoupled.
    virtual bool RegisterBackend(AZStd::unique_ptr<IBackend> backend) = 0;
    virtual void UnregisterBackend(const AZ::Name& name) = 0;

    // Installs an action verb, which is how a module contributes vocabulary.
    virtual ActionStateId RegisterAction(AZStd::unique_ptr<IActionState> action) = 0;
    virtual void UnregisterAction(ActionStateId id) = 0;

    // Names of what is currently installed, for console output and validation.
    virtual AZStd::vector<AZ::Name> GetBackendNames() const = 0;
    virtual AZStd::vector<AZ::Name> GetActionNames() const = 0;
    virtual AZStd::vector<AZ::Name> GetTreeNames() const = 0;
};
```

This interface is the **only public API** a module needs to extend G.O.A.T.

---

## ⚖️ Trade-offs

| ✅ Advantage | ⚠️ Disadvantage |
| :--- | :--- |
| No core modification for new features | Requires understanding the interfaces |
| Plug-and-play behavior | More moving parts to debug |
| Designer-friendly vocabulary | More documentation needed |
| Runtime registration/unregistration | Slight overhead for registry lookups |
| Clean separation of concerns | Slight learning curve for new contributors |

---

## 🧩 Impact on the Codebase

### Lua Layer

- Designers use new verbs directly in trees (`script "MoveTo"`).
- Designers define custom backends in Lua (`backend "MyGoap" { ... }`).
- Designers can write custom control flow (`flow "MyComposite" { ... }`).

### C++ Core

- `ActionStateRegistry` stores and dispatches `IActionState`s.
- `BackendRegistry` stores and dispatches `IBackend`s.
- `NodeTypeRegistry` stores descriptors used by `TreeCompiler`.

### Modules

- **Navigation Module:** Registers `MoveTo`, `Wander`, etc. as `IActionState`s.
- **Animation Module:** Registers `PlayAnimation`, `SetAnimationState`, etc.
- **GOAP Backend:** Implements `IBackend` for goal-oriented planning.
- **Director AI Backend:** Implements `IBackend` for global orchestration.

---

## 🗺️ Future Evolution

### Navigation Library

- Will register `MoveTo`, `FollowPath`, `Wander`, `Flee`, etc., as `IActionState`s.
- Will expose them to Lua via `BehaviorContext` so trees can use `script "MoveTo"`.

### Perception Module

- Will likely be implemented as Lua `service` nodes, not C++ components.
- Will write perception results to blackboard keys (e.g., `TargetVisible`, `TargetDistance`).

### Custom Node Types

- Modules can register new node types via `NodeTypeRegistry`.
- Trees can use these node types directly in Lua.

---

## 🔗 Related Concepts

- [[Design Principles]]
- [[Backend Abstraction Theory]]
- [[Performance Model]]
- [[Adding New Actions]]

---

*Last updated: 2026-08-26*