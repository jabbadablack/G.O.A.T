---
type: theory
status: implemented
tags: [architecture, design, philosophy]
---

# Backend Abstraction Theory

> **Status:** Implemented
> **Core files:** `Code/Include/GOAT/Interfaces/IDecisionBackend.h`, `Code/Include/GOAT/Interfaces/IBackend.h`

---

## The idea

Most AI frameworks pick a paradigm and build everything around it. You get a behaviour tree
framework, or an HTN framework, and swapping means starting over.

GOAT does not pick. The core knows about **agents, a blackboard, plans, and actions** — and
nothing about how decisions are made. A paradigm is a backend, a backend is a gem, and a project
that only wants task networks deletes the behaviour tree gem.

The test of whether that is real: `grep -r 'BehaviorTree\|Htn' Code/Source/Core/` returns nothing.

---

## The contract

```cpp
class IDecisionBackend
{
public:
    virtual AZ::Name GetName() const = 0;
    virtual AZStd::vector<AZ::Name> GetNodeTypes() const = 0;
    virtual CompileOutcome Compile(const AZ::Name& name, const AuthoredNode& root) = 0;
    virtual size_t GetStateSize() const = 0;

    virtual void Attach(const PlanContext&, const AgentProgram&, BrainState);
    virtual TickResult Advance(const PlanContext&, const AgentProgram&, BrainState,
                               float elapsed, size_t runningStep);
    virtual Decision Decide(const PlanContext&, const AgentProgram&, BrainState,
                            ActionResult lastResult, float elapsed, ActionPlan& outPlan);
    virtual void Release(const PlanContext&);
};
```

Strip it down and every paradigm answers the same two questions:

- **`Decide` — what next?**
- **`Advance` — does what you are doing still hold?**

A behaviour tree answers the first by walking to the next runnable leaf and the second by
re-checking guards. A task network answers the first by decomposing tasks and the second by
re-validating the remaining primitives. GOAP would answer them by searching and by checking
preconditions. Same two questions.

---

## What makes it work: the runtime owns the plan

`Advance` returns `Continue` or `Abandon`. That is all a backend can say about a running plan.

It cannot end a step early, cannot run one itself, cannot reach `AgentStateMachine`. The
[[PlanContext]] it is handed contains the agent, the entity, the blackboard, optional scripting,
and the plan store — deliberately not the agent registry or the state machine.

This is the constraint that does the real work. Because no backend can reach into execution, two
of them can run in the same level on different agents without knowing about each other. Loosen it
and paradigms stop composing.

---

## What makes them interoperate: the blackboard

The two paradigms in the Scry test bench never call each other. A task network writes
`crowd_pace`; a hundred behaviour trees read it. Nothing is wired up.

Two things make that work without polling:

- A declared `condition` **is** a dependency. Compiling one records the scope it reads, so the
  agent is woken when that scope changes. You do not write `abort` to get this; it is the default.
- [[GuardWatch]] counts changes per scope rather than registering callbacks. One global write is
  one increment, not a walk of the level.

So "the director changed the pace and the crowd reacted" costs one integer write. That is the
whole integration story between paradigms.

---

## Per-agent state, without the core knowing what it is

```cpp
using BrainState = AZStd::span<AZ::u8>;
```

A backend says how many bytes it needs with `GetStateSize()`. The runtime carves that many out of
the agent record and hands the span back on every call. The core never looks inside.

A behaviour tree keeps a `DecisionCursor` there. A task network keeps the task indices of the
plan it is running. Neither type appears anywhere in the core.

---

## The other IBackend

[[IBackend]] is a second, narrower seam and it still exists:

```cpp
virtual bool Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan) = 0;
```

One intent in, one plan out. It is what a `delegate` leaf reaches, and [[LuaBackend]] implements
it so a planner can be written in Lua without touching C++.

The two are not competing. `IDecisionBackend` is *how this agent thinks*; `IBackend` is *who
satisfies this one request*. A behaviour tree that delegates to a Lua planner is using both at
once.

---

## What it costs

Honestly: one virtual call per agent per decision, and a compile step per paradigm that is
compiled once and shared by every agent running it.

`BM_TickBand` measures about **13 ns per agent** for a full band tick, and an agent record is
**248 bytes**. The abstraction is not where the time goes.

---

## The two that ship

| Backend | Name | Gem | Program type |
| :--- | :--- | :--- | :--- |
| `BehaviorTreeBackend` | `tree` | GOAT_BehaviorTree | [[DecisionProgram]] |
| `HtnBackend` | `htn` | GOAT_Htn | `HtnDomain` |

Both are ordinary gems. Neither is referenced by name anywhere in the core.

---

## Related

- [[IDecisionBackend]]
- [[IBackend]]
- [[Extensibility Model]]
- [[Layered Overview]]
- [[Design Principles]]
- [[Performance Model]]

---

*Last updated: 2026-08-27*
