---
type: component
status: active
tags: [cpp, core, interface, extensibility, backend]
---

# IDecisionBackend

> **Header:** `Code/Include/GOAT/Interfaces/IDecisionBackend.h`
> **Kind:** Extension interface, public API

---

## Overview

**Decides how an agent acts. One of these per paradigm.**

This is the seam that makes GOAT paradigm-neutral. A behaviour tree is one implementation; a
hierarchical task network is another; both ship as gems you can remove. The core owns no opinion
about what a `selector` or a `method` means — a backend claims the words it understands and
compiles them.

Two ship today:

| Backend | Name | Gem |
| :--- | :--- | :--- |
| `BehaviorTreeBackend` | `tree` | GOAT_BehaviorTree |
| `HtnBackend` | `htn` | GOAT_Htn |

An agent picks one with the **Brain** field on its component.

---

## The interface

```cpp
AZ::Name GetName() const;
AZStd::vector<AZ::Name> GetNodeTypes() const;
CompileOutcome Compile(const AZ::Name& name, const AuthoredNode& root);
size_t GetStateSize() const;

void Attach(const PlanContext&, const AgentProgram&, BrainState);
TickResult Advance(const PlanContext&, const AgentProgram&, BrainState, float elapsed, size_t runningStep);
Decision Decide(const PlanContext&, const AgentProgram&, BrainState, ActionResult lastResult,
                float elapsed, ActionPlan& outPlan);
void Release(const PlanContext&);
```

`Attach`, `Advance` and `Release` have defaults, so a simple backend implements four methods.

---

## What each one is for

**`Compile`** turns an [[AuthoredNode]] into an [[AgentProgram]] — a compiled thing shared by
every agent running it. Returns the reason on failure, so a bad program is a build error with a
message rather than a runtime surprise.

**`GetStateSize`** is how many bytes of per-agent state the backend needs. The runtime carves
that out and hands it back as a `BrainState`, which is just `span<AZ::u8>`. The backend decides
what is in it; the core never looks.

**`Decide`** produces the next plan, given how the last one ended. It writes into an
[[ActionPlan]], which is a span into the [[PlanStore]], so a plan has no length limit and costs
no copy.

**`Advance`** re-checks whatever could interrupt the agent and runs any periodic work. It is
called **only** when a scope the program watches has changed, or when the program asked to be
ticked every frame — that is what keeps an idle crowd nearly free. It answers `Continue` or
`Abandon`:

```cpp
enum class TickResult : AZ::u8
{
    Continue, //!< Leave it running.
    Abandon   //!< Drop it and decide again.
};
```

That is the whole contract for interruption. A backend never touches the state machine; it says
drop the plan, and the runtime does the rest.

---

## The contract that keeps this simple

**The runtime owns the plan.** A backend produces steps and says whether they still stand. It
cannot reach into `AgentStateMachine`, cannot run a step itself, and cannot end one early. Every
paradigm therefore reduces to the same two questions — *what next?* and *does it still hold?* —
which is what lets two paradigms coexist on the same agents.

---

## Registering one

Through `GOATBackendRequestBus`, which takes ownership:

```cpp
AZStd::unique_ptr<IDecisionBackend> backend = AZStd::make_unique<MyBackend>(...);
bool registered = false;
GOATBackendRequestBus::BroadcastResult(
    registered, &GOATBackendRequests::RegisterDecisionBackend, backend);
```

The parameter is a reference to the `unique_ptr` so ownership survives the bus. Names must be
unique; registering a taken one fails and says so.

---

## Not the same as IBackend

[[IBackend]] is a different, older seam: it turns a single `delegate` **intent** into a plan, and
[[LuaBackend]] implements it so a planner can be written in Lua. `IDecisionBackend` decides for a
whole agent, every tick. Both exist; they answer different questions.

---

## Related

- [[AgentProgram]]
- [[PlanContext]]
- [[PlanStore]]
- [[AuthoredNode]]
- [[Backend Abstraction Theory]]
- [[Writing Custom Backends]]

---

*Last updated: 2026-08-27*
