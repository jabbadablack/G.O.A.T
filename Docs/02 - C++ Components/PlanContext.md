---
type: component
status: active
tags: [cpp, core, domain, backend]
---

# PlanContext

> **Header:** `Code/Include/GOAT/Domain/PlanContext.h`
> **Kind:** Plain struct, public API

---

## Overview

Everything a backend may reach while planning for one agent — and, just as importantly, nothing
else.

```cpp
struct PlanContext final
{
    AgentId m_agent;
    AZ::EntityId m_entity;
    IBlackboardSystem* m_blackboard = nullptr;
    INodeScripting* m_scripting = nullptr;
    PlanStore* m_planStore = nullptr;
};
```

Every method on [[IDecisionBackend]] takes one. It is built fresh per call and never stored.

---

## Why it is this small

The list is the backend sandbox. A backend gets the agent it is planning for, the entity behind
it, the shared blackboard, optional user-defined control flow, and somewhere to put steps.

It does **not** get the agent registry, the state machine, or the other agents. That is
deliberate: the runtime owns the plan, and a backend that could reach the state machine could end
a step early or run one itself, which is exactly the coupling that would stop two paradigms
coexisting.

The blackboard is the only thing a backend and everything else have in common. That is what makes
paradigms interoperable without wiring — a task network writes a variable, a behaviour tree reads
it, and neither knows the other exists.

---

## Related

- [[IDecisionBackend]]
- [[PlanStore]]
- [[IBlackboardSystem]]
- [[INodeScripting]]

---

*Last updated: 2026-08-27*
