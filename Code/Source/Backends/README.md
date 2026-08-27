# Backends

A backend is the middle stage of the pipeline:

```
Lua BT  ──►  Backend  ──►  FSM  ──►  Agent
             (here)
```

The tree emits an `Intent` — *what* the agent should achieve. A backend turns that into an
`ActionPlan` — a short sequence of action verbs the agent's state machine runs one at a time.
This is F.E.A.R.'s architecture: goal → planner → plan → small FSM → agent.

Nothing outside this folder names a concrete backend. Deleting a backend's folder removes it,
along with any verbs it registered.

## Adding one

Implement `IBackend` (`Include/GOAT/Interfaces/IBackend.h`) — four methods, only two required:

```cpp
class HtnBackend final : public IBackend
{
public:
    AZ::Name GetName() const override { return AZ_NAME_LITERAL("htn"); }

    bool Plan(const PlanContext& context, const Intent& intent, ActionPlan& outPlan) override;

    // Optional: conditions that invalidate the plan while it runs.
    void CollectGuards(const PlanContext&, const ActionPlan&, GuardList&) const override;

    // Optional: release any per agent state.
    void Release(const PlanContext&) override;
};
```

Register it through `IAgentSystem`:

```cpp
AgentSystemInterface::Get()->RegisterBackend(AZStd::make_unique<HtnBackend>());
```

A tree reaches it by name:

```lua
delegate "htn" { goal = "SecurePerimeter" }
```

## Adding a verb

If a backend needs a verb the core does not have — a Bark backend needs "say a line", and none of
`wait` or `script` means that — register an `IActionState` alongside it:

```cpp
AgentSystemInterface::Get()->RegisterAction(AZStd::make_unique<SayAction>());
```

The FSM dispatches on a registered index, so this costs nothing at runtime and the verb disappears
with the backend.

## Writing one in Lua instead

A backend does not have to be C++:

```lua
backend "MyGoap" {
  plan = function(me, intent)
    return { { action = "script", behavior = "Approach" },
             { action = "wait",   seconds = 0.5 } }
  end,
}
```

## What a backend may reach

Only what `PlanContext` hands it: the agent id, its entity, and the blackboard. The blackboard is
the single thing every stage shares, which is what keeps backends swappable — a backend never sees
the tree, the state machine, or another backend.

## The direct backend is not one of these

`Source/Core/Frontend/DirectBackend.cpp` turns a plainly authored leaf into a one step plan. It
lives in the frontend rather than here because the pipeline is incomplete without it: it is what
lets a tree run end to end with no backend installed at all.
