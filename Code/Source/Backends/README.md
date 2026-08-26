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

A backend does not have to be C++. In practice none of them are: every backend that ships is
written in Lua, and the C++ interface exists for a project that needs a planner fast enough to
justify it.

```lua
backend "MyGoap" {
  plan = function(me, ctx, goal)
    return { { action = "script", behavior = "Approach" },
             { action = "wait",   seconds = 0.5 } }
  end,
}
```

A backend may instead implement `choose(me, ctx, goal, builder)` and drive the builder itself,
which is what lets one hand back steps it has already had baked rather than pushing them again.

## The `bt` backend, and declaring plans

`Assets/GOAT/Backends/BehaviorTreeBackend.lua` ships with the gem and satisfies any goal declared
with `plan`. It is loaded with the vocabulary, so a tree may delegate to it without the project
declaring anything.

```lua
plan "SecurePerimeter" {
  option {
    when = "has_cover",
    { action = "move_to", key = "cover_pos", tolerance = 0.5 },
    { action = "script",  behavior = "Suppress" },
  },
  option {
    unless = "suppressed",
    { action = "script", behavior = "Advance" },
  },
  option {                                  -- no guard: the fallback, and it must be last
    { action = "wait", seconds = 0.5 },
  },
}
```

```lua
delegate "bt" { goal = "SecurePerimeter" }
```

Options are tried in order and the first whose guard holds contributes **all** of its steps. An
option is all or nothing; a guard that fails contributes nothing at all.

An ordered list of guarded step lists is a behaviour tree — it is exactly
`selector { sequence {...}, sequence {...} }` restricted to two levels. What it adds over writing
that in the tree itself is **commitment**: the whole sequence reaches the state machine as one
plan, and the tree is not consulted again until the plan finishes or a guard aborts it.

| Step key | Sets |
|---|---|
| `action` | The verb to run. Defaults to `script` |
| `behavior` / `tag` | The thing to run: a Lua behaviour, an animation, a line |
| `seconds` | The one scalar a verb needs -- a duration for `wait`, a speed for `move_to` |
| `tolerance` | How close counts as arrived |
| `key` | A blackboard variable holding the target |
| `at` | A literal `Vector3` target |
| `entity` | A literal entity, only reachable from the imperative form |

**A guard is a blackboard bool name, not an expression.** That is what lets every plan be checked
when the file loads rather than when an agent first delegates to it: a name can be resolved against
the declared variables, a closure can only be checked by calling it, which needs an agent that does
not exist yet. Anyone who needs `ammo > 0` writes an imperative `backend` instead, which has always
been able to do anything.

## Invalidating a plan while it runs

A backend does not report guards. It does not need to: the tree's own guards cover a running plan,
because the `delegate` node stays the agent's active leaf for the plan's whole lifetime.

```lua
sequence {
  condition "has_cover" { abort = "self" },
  delegate "bt" { goal = "SecurePerimeter" },
}
```

Flipping `has_cover` aborts the plan mid step, ends the running verb so it gives back whatever it
held, and returns control to the tree. Guard the delegate node, not the plan.

## A plan cannot re-enter the tree

A step names a *verb*. `delegate` is a tree word, never a verb, and nothing may register one under
that name — so a plan structurally cannot delegate back to the tree that asked for it. The
validator says so explicitly if a plan tries.

## Checking and inspecting

| Command | Prints |
|---|---|
| `GOATSystemComponent.ListPlans` | Every plan, its option count, and where it was declared |
| `GOATSystemComponent.DumpPlan <name>` | One plan's options, their guards and their steps |
| `GOATSystemComponent.ValidatePlans` | Re-checks every plan and reports what is wrong |

`LoggerSystemComponent.EnableLog GoatPlan` reports which option won and why a plan came back
empty — the inside of a backend call, which the `GoatAgent` channel cannot see.

## What a backend may reach

Only what `PlanContext` hands it: the agent id, its entity, and the blackboard. The blackboard is
the single thing every stage shares, which is what keeps backends swappable — a backend never sees
the tree, the state machine, or another backend.

## The direct backend is not one of these

`Source/Core/Frontend/DirectBackend.cpp` turns a plainly authored leaf into a one step plan. It
lives in the frontend rather than here because the pipeline is incomplete without it: it is what
lets a tree run end to end with no backend installed at all.
