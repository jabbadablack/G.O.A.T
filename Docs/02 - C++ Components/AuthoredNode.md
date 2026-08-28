---
type: component
status: active
tags: [cpp, core, asset, authoring]
---

# AuthoredNode

> **Header:** `Code/Include/GOAT/Assets/BehaviorTreeAsset.h`
> **Kind:** Plain struct, public API

---

## Overview

One node exactly as it was authored, before any backend has looked at it. This is the shape Lua
builds in memory and the shape a graph editor would save to disk.

It is **paradigm-neutral**. The same struct carries a behaviour tree's `selector` and a task
network's `method`; what those words mean is decided by whichever backend compiles it. That is
why the type is a plain bag of a name, some properties and some children rather than anything
tree-shaped.

```cpp
struct AuthoredNode final
{
    //! Node type name, resolved against the node type registry when compiled.
    AZStd::string m_type;
    //! Authored properties for this node.
    AZStd::vector<AuthoredProperty> m_properties;
    //! Services attached to this node, valid only on composites.
    AZStd::vector<AuthoredNode> m_services;
    //! Children, in execution order.
    AZStd::vector<AuthoredNode> m_children;
    //! Ignored at runtime.
    AuthoredNodeMetadata m_metadata;
};
```

Children are in execution order, left to right. Nothing reorders them.

---

## Where it comes from and goes

Lua builds one through [[LuaTreeBuilder]]. `IDecisionBackend::Compile` takes it and returns an
[[AgentProgram]] — a [[DecisionProgram]] from the behaviour tree gem, an `HtnDomain` from the task
network gem. Nothing at runtime ever sees an `AuthoredNode` again.

---

## Related

- [[AuthoredProperty]]
- [[AuthoredNodeMetadata]]
- [[BehaviorTreeAsset]]
- [[IDecisionBackend]]
- [[LuaTreeBuilder]]

---

*Last updated: 2026-08-27*
