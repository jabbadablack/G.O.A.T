---
type: component
status: active
tags: [cpp, core, component]
---

# GOATSystemComponent

> **File Location:** `Code/Source/Clients/GOATSystemComponent.cpp`  
> **Header:** `Code/Source/Clients/GOATSystemComponent.h`  
> **Inherits:** `AZ::Component`, `IAgentSystem`, `GOATRequestBus::Handler`, `AzFramework::AssetCatalogEventBus::Handler`

---

## Overview

`GOATSystemComponent` is the **central "God Object"** of the G.O.A.T. framework. It is an O3DE system component that owns and manages the lifecycle of every core service. It implements the `IAgentSystem` interface, which is the single public API that game modules, backends, and other gems use to extend the framework.

It is responsible for initializing the blackboard schema, registering core actions (`Wait`, `RunScript`), loading the Lua authoring vocabulary, managing the `AgentRegistry`, and bridging the Asset Catalog to ensure scripts are loaded only when the engine is ready.

---

## Key Responsibilities

| # | Responsibility | Description |
| :--- | :--- | :--- |
| 1 | **Service Initialization** | Creates and owns `BlackboardSystem`, `ActionStateRegistry`, `BackendRegistry`, `NodeTypeRegistry`, `TreeLibrary`, `LuaDispatch`, `AgentRuntime`, and `AgentRegistry`. |
| 2 | **Lua Vocabulary Loading** | Searches the Asset Catalog for `goat/scripts/goat.luac` or `goat.lua`, loads it, and runs it to populate the global DSL. |
| 3 | **Agent Management** | Implements `IAgentSystem` methods to compile trees, register/unregister agents, and join squads. |
| 4 | **Console Tooling** | Registers `AZ::Console` commands (`ListBackends`, `ListAgents`, `DumpAgent`, etc.) for runtime debugging. |
| 5 | **Lua Backend Registration** | Detects backends defined in Lua (via `GOAT_HasBackend`) and installs `LuaBackend` wrappers automatically. |
| 6 | **Asset Handler Registration** | Registers `BlackboardAssetHandler` for `.bbx` assets. |
| 7 | **Vocabulary Reload** | Provides a console command to reload the Lua vocabulary after asset catalog changes. |

---

## Public Interface

### IAgentSystem Methods

```cpp
bool LoadScript(const AZ::Data::Asset<AZ::ScriptAsset>& asset) override;
AZ::Outcome<void, AZStd::string> LoadBlackboard(const BlackboardAsset& asset) override;
AZ::Outcome<void, AZStd::string> CompileProgram(
    const AZ::Name& backendName, const AZ::Name& programName) override;
AgentId RegisterAgent(AZ::EntityId entity, const AZ::Name& treeName, size_t band) override;
void UnregisterAgent(AgentId agent) override;
void JoinSquad(AgentId agent, const AZ::Name& squad) override;
bool RegisterBackend(AZStd::unique_ptr<IBackend> backend) override;
void UnregisterBackend(const AZ::Name& name) override;
ActionStateId RegisterAction(AZStd::unique_ptr<IActionState> action) override;
void UnregisterAction(ActionStateId id) override;
AZStd::vector<AZ::Name> GetBackendNames() const override;
AZStd::vector<AZ::Name> GetActionNames() const override;
AZStd::vector<AZ::Name> GetTreeNames() const override;
AZStd::string DescribeAgent(AgentId agent) const override;
```

### Console Commands

```cpp
void ListBackends(const AZ::ConsoleCommandContainer& arguments);
void ListActions(const AZ::ConsoleCommandContainer& arguments);
void ListNodes(const AZ::ConsoleCommandContainer& arguments);
void ListTrees(const AZ::ConsoleCommandContainer& arguments);
void ListAgents(const AZ::ConsoleCommandContainer& arguments);
void DumpAgent(const AZ::ConsoleCommandContainer& arguments);
void ReloadVocabulary(const AZ::ConsoleCommandContainer& arguments);
```

---

## Dependencies & Interactions

```mermaid
graph LR
    A[GOATSystemComponent] --> B[IAgentSystem]
    A --> C[LuaDispatch]
    A --> D[BlackboardSystem]
    A --> E[ActionStateRegistry]
    A --> F[AgentRuntime]
    A --> G[AgentRegistry]
    A --> H[Asset Catalog]
    A --> I[NodeTypeRegistry]
    A --> J[TreeLibrary]
    A --> K[BackendRegistry]
```

- **Depends on:** `ScriptService`, `AssetDatabaseService` (declared in `GetDependentServices`).
- **Required by:** `GOATAgentComponent` (via `IAgentSystem` interface), `GOATEditorSystemComponent`.
- **Interacts with:** `LuaDispatch`, `TreeCompiler`, `TreeWalker`, `AgentRuntime`, `BlackboardSystem`, `ActionStateRegistry`, `BackendRegistry`, `NodeTypeRegistry`, `TreeLibrary`.

---

## Implementation Notes

### Key Algorithms

#### `StartServices()`

Enforces a **strict initialization order**:

1. Create `BlackboardSystem`, `ActionStateRegistry`, `BackendRegistry`, `NodeTypeRegistry`, `TreeLibrary`.
2. Create `LuaDispatch` and `AgentScriptContext`.
3. Register the core `IActionState`s (`WaitAction`, `RunScriptAction`) and the director verbs.
4. Create `LuaNodeScripting`, `AgentRuntime`, and `AgentRegistry`.
5. Configure the `LuaPlanBuilder`.
6. Connect `LuaDispatch` to the script context.

```cpp
// Code/Source/Clients/GOATSystemComponent.cpp
void GOATSystemComponent::StartServices()
{
    m_blackboardSystem = AZStd::make_unique<BlackboardSystem>();
    m_actions = AZStd::make_unique<ActionStateRegistry>();
    m_backends = AZStd::make_unique<BackendRegistry>();
    m_nodeTypes = AZStd::make_unique<NodeTypeRegistry>();
    m_trees = AZStd::make_unique<TreeLibrary>();
    m_dispatch = AZStd::make_unique<LuaDispatch>();
    m_scriptContext = AZStd::make_unique<AgentScriptContext>();

    m_directBackend = AZStd::move(direct);

    m_actions->RegisterAt(CoreActions::Wait, AZStd::make_unique<WaitAction>());
    m_actions->RegisterAt(CoreActions::RunScript, AZStd::make_unique<RunScriptAction>(*m_dispatch, *m_scriptContext));

    m_scripting = AZStd::make_unique<LuaNodeScripting>(*m_dispatch, *m_scriptContext);
    m_runtime = AZStd::make_unique<AgentRuntime>(
        *m_blackboardSystem, *m_actions, *m_backends, *m_directBackend, *m_dispatch, *m_scriptContext,
        *m_scripting);
    m_agents = AZStd::make_unique<AgentRegistry>(*m_runtime, *m_blackboardSystem, *m_dispatch);

    m_dispatch->ConfigurePlanBuilder(m_actions.get(), m_blackboardSystem.get());
    m_vocabularyLoaded = false;
    m_dispatch->Connect();
}
```

#### Asset Catalog Deferred Loading

`EnsureVocabulary()` is called by `OnCatalogLoaded`. This is crucial because system components activate *before* the Asset Catalog is ready. If the vocabulary fails to load at `Activate()`, it retries once the catalog is loaded, ensuring the DSL is always available.

#### `LoadVocabulary()`

Searches for the compiled `.luac` file first, then falls back to the raw `.lua` file:

```cpp
constexpr const char* VocabularyAssetPaths[] = {
    "goat/scripts/goat.luac",
    "goat/scripts/goat.lua",
};
```

---

### Performance Considerations

- **Allocation:** All heavy services are created once during `StartServices()` and destroyed during `StopServices()`.
- **Tick Rate:** The component itself does not tick. It only registers agents with the `AgentRuntime`, which handles scheduling.
- **Concurrency:** Runs entirely on the main thread.

---

## Lua Exposure

`GOATSystemComponent` is not directly exposed to Lua. Instead, it loads `GOAT.lua` (the vocabulary) into the script context and exposes the runtime services (like `LuaDispatch`, `LuaTreeBuilder`, `LuaPlanBuilder`) to Lua via `BehaviorContext::Reflect()`.

---

## Testing

Unit tests should cover:

- `StartServices` / `StopServices` lifecycle without crashes.
- `LoadVocabulary` correctly falls back to `goat.lua` when `goat.luac` is missing.
- `RegisterAction` and `RegisterBackend` correctly update their respective registries.
- `CompileProgram` correctly adds programs to the map and fails on invalid trees.
- `LoadBlackboard` correctly declares variables from a `.bbx` asset.
- `RegisterAgent` correctly creates an agent and joins a squad if specified.

---

## Related Notes

- [[GOATAgentComponent]]
- [[LuaDispatch]]
- [[TreeCompiler]]
- [[TreeWalker]]
- [[AgentRegistry]]
- [[AgentRuntime]]
- [[Layered Overview]]
- [[GOATEditorSystemComponent]]

---

*Last updated: 2026-08-26*