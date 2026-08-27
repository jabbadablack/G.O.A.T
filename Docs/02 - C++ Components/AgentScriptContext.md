
# AgentScriptContext

> **File Location:** `Code/Source/Core/Scripting/AgentScriptContext.cpp`  
> **Header:** `Code/Source/Core/Scripting/AgentScriptContext.h`  
> **Inherits:** None (Plain class, exposed to Lua via `BehaviorContext`)

---

## Overview

`AgentScriptContext` is the **C++ side of the Lua context** that provides type-safe access to the blackboard for Lua behaviors. It is passed as the `ctx` argument to every `behavior` `tick` function. It wraps the `IBlackboardSystem` and provides methods for reading/writing typed variables.

---

## Key Responsibilities

| # | Responsibility | Description |
| :--- | :--- | :--- |
| 1 | **Blackboard Access** | Provides `SetBool`, `GetBool`, `SetInt`, `GetInt`, etc. for Lua scripts. |
| 2 | **Context Binding** | Binds the context to a specific agent, entity, and blackboard before a Lua call. |
| 3 | **Reflection** | Exposes the context to Lua via `BehaviorContext`. |

---

## Public Interface

### Methods

```cpp
// Binds the context to a specific agent and entity.
void Bind(AgentId agent, AZ::EntityId entity, IBlackboardSystem* blackboard);

// Unbinds the context.
void Unbind();

// Lua-exposed methods for blackboard access.
void SetBool(const AZStd::string& name, bool value);
bool GetBool(const AZStd::string& name) const;
void SetInt(const AZStd::string& name, AZ::s64 value);
AZ::s64 GetInt(const AZStd::string& name) const;
void SetFloat(const AZStd::string& name, float value);
float GetFloat(const AZStd::string& name) const;
void SetVector3(const AZStd::string& name, const AZ::Vector3& value);
AZ::Vector3 GetVector3(const AZStd::string& name) const;
```

---

## Dependencies & Interactions

```mermaid
graph LR
    A[Lua Behavior] -->|ctx| B[AgentScriptContext]
    B --> C[IBlackboardSystem]
    C --> D[BlackboardStorage]
```

- **Depends on:** `IBlackboardSystem` (to access storage).
- **Required by:** `LuaDispatch` (to pass to behaviors), `LuaBackend` (to bind before planning).

---

## Implementation Notes

### Key Algorithms

`Bind()` stores the current agent, entity, and blackboard pointers. All `Set`/`Get` methods resolve the variable name to a `BlackboardKey` via `IBlackboardSystem::FindKey()` and then access the appropriate storage.

### Performance Considerations

- **Allocation:** No per-call allocations.
- **Tick Rate:** Called on every `GOAT_Dispatch` tick.
- **Concurrency:** Main thread only.

---

## Lua Exposure

Directly exposed to Lua as the `ctx` argument in behavior `tick` functions.

```lua
behavior "Patrol" {
    tick = function(me, ctx)
        ctx:SetInt("patrol_stop", me.stop)
        return SUCCESS
    end,
}
```

---

## Testing

Unit tests should cover:

- **Set/Get:** Correctly reading and writing typed values.
- **Invalid Key:** Failing gracefully when an undeclared variable is accessed.
- **Bind/Unbind:** Correctly isolating state between agents.

---

## Related Notes

- [[LuaDispatch]]
- [[BlackboardSystem]]
- [[Behavior DSL]]

---

*Last updated: 2026-08-26*