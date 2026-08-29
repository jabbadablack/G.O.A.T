---
type: component
status: active
tags: [cpp, core, debugging]
---

# AgentSnapshot

> **Header:** `Code/Include/GOAT/Domain/AgentDebug.h`
> **Kind:** Plain struct, public API

---

## Overview

Everything a tool shows about one agent, gathered in a single pass. It is what
[[DescribeAgent]] prints as a line, kept as fields instead of as text, so a panel can sort a
column and a wire can carry it.

```cpp
struct AgentSnapshot final
{
    AZ::u32 m_agentIndex;
    AZ::u32 m_agentGeneration;
    AZ::EntityId m_entity;
    AZ::Name m_program;
    AZ::Name m_backend;
    AZ::Name m_squad;
    AZ::Name m_action;
    AZ::u8 m_band;
    AZ::u32 m_step;
    AZ::u32 m_planSize;
    float m_elapsed;
    AZ::u32 m_interrupted;
    AZStd::vector<ProgramNodeRef> m_activePath;
};
```

The agent is carried as its index and generation rather than as an [[AgentId]], because a
snapshot also travels to another process, and those two numbers are the whole of a handle's
public surface. `GetAgent()` puts one back together.

`DescribeAgent` is built on top of this, so the console and a panel cannot disagree about what
an agent is doing.

---

## The active path

`m_activePath` is where the agent is inside its program, root first, ending at whatever is
actually running. Each step is a [[ProgramNodeRef]].

The core does not and cannot work this out. An agent's position lives in
[[AgentRecord]]`::m_brain`, an opaque block *"in whatever shape its backend chose"*. So
`SnapshotAgent` asks the agent's own backend through
[[IDecisionBackend]]`::DescribePosition`, and copies down whatever it says.

An empty path is not a fault. It means the backend running this agent has not implemented
`DescribePosition`, or the agent is idle. A backend that says nothing shows no highlight,
which is better than a wrong one.

---

## Where it comes from

| Method | Does |
| :--- | :--- |
| `IAgentSystem::SnapshotAgent(agent, out)` | One agent. False when there is no such agent. |
| `IAgentSystem::SnapshotAgents()` | Every registered agent, which is what a panel polls. |

Polled rather than pushed: nothing is recorded as an agent runs, so a runtime nobody is
watching pays nothing for this. See [[Debugging Agents]].

---

## Related Notes

- [[ProgramNodeRef]]
- [[AgentRecord]]
- [[IDecisionBackend]]
- [[Debugging Agents]]
