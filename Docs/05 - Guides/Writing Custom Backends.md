---
type: guide
status: active
tags: [guide, tutorial, cpp, backend]
---

# Writing Custom Backends

> **Time:** an afternoon for a real one
> **You should have read:** [[Backend Abstraction Theory]], [[IDecisionBackend]]

---

## Which kind do you want?

Two different jobs share the word "backend". Pick before you start.

**A whole paradigm.** GOAP, a state machine — something that decides how an agent acts, every
tick, instead of a behaviour tree. That is [[IDecisionBackend]], it is C++, and it is the rest of
this guide. [[Utility AI]] is the most recent one written this way, and its gem is the smallest
worked example of everything below.

**A planner behind one `delegate` leaf.** Something that turns one goal into a few steps. That is
[[IBackend]], and you can write it in Lua in about ten lines — see [[Backends]]. Do that instead
if it fits; it is far less work.

---

## Step 1 — Make a gem

A paradigm belongs in its own gem, so a project that does not want it can delete it. Copy the
shape of `Code/Source/Backends/Htn/`:

```
Code/Source/Backends/MyParadigm/
├── gem.json
├── CMakeLists.txt
├── Registry/assetprocessor_settings.setreg
├── Assets/GOAT_MyParadigm/Scripts/MyParadigm.lua
└── Code/
    ├── CMakeLists.txt
    ├── goat_myparadigm_*_files.cmake
    ├── Platform/
    └── Source/
        ├── Clients/GOAT_MyParadigmSystemComponent.{h,cpp}
        ├── Tools/GOAT_MyParadigmEditorSystemComponent.{h,cpp}
        └── MyParadigmBackend.{h,cpp}
```

Then add it to the root `gem.json`'s `external_subdirectories`.

> **Don't skip the Editor variant.** A gem with no Tools target is invisible to the Asset
> Processor, so its scan folder is never merged and its Lua never compiles. It also won't load in
> the Editor at all. This costs an hour to diagnose and a minute to avoid.

---

## Step 2 — Decide what per-agent state you need

```cpp
struct MyState final
{
    AZ::u16 m_currentGoal = 0;
    float m_lastScore = 0.0f;
};

size_t GetStateSize() const override { return sizeof(MyState); }
```

The runtime carves that many bytes out of the agent record and hands them back as a
`BrainState`, which is just `span<AZ::u8>`. The core never looks inside.

Keep it small. It is per agent, and the whole record is 248 bytes today.

---

## Step 3 — Claim your words

```cpp
AZStd::vector<AZ::Name> GetNodeTypes() const override
{
    return { AZ::Name("consideration"), AZ::Name("scorer") };
}
```

These are the words your backend gives meaning to. Then declare them in Lua so authors can type
them:

```lua
-- Assets/GOAT_MyParadigm/Scripts/MyParadigm.lua
GOAT_DeclareNode("state", "name")
GOAT_DeclareNode("transition", "to")

function machine(name)
    return function(body)
        local compiled = GOAT.Compile(name, GOAT.nodeType("machine")(body))
        -- Without this nothing can look the program up, and GOAT_EmitTree will never find it.
        GOAT._trees[name] = compiled
        return compiled
    end
end
```

`GOAT_DeclareNode` auto-declares a global word for a registered node type, with its main property
first — so most words need one line. Register the file at startup:

```cpp
agents->RegisterVocabularyScript("goat_myparadigm/scripts/myparadigm");
```

Do **not** put your words in `GOAT.lua`. That file holds only what every paradigm shares.

---

## Step 4 — Compile

```cpp
CompileOutcome Compile(const AZ::Name& name, const AuthoredNode& root) override
{
    auto program = AZStd::shared_ptr<MyProgram>(aznew MyProgram());
    program->m_name = name;
    program->m_backend = this;

    if (!Flatten(root, *program))
    {
        return AZ::Failure(AZStd::string::format("'%s' has no considerations", name.GetCStr()));
    }

    // Which scopes we guard on. Anything else must never wake us.
    program->m_watchedScopes[static_cast<size_t>(BlackboardScope::Global)] = true;

    return AZ::Success(AZStd::shared_ptr<AgentProgram>(AZStd::move(program)));
}
```

Two things to get right.

**Fail with a reason.** `CompileOutcome` carries a string. An author reading "'Patrol' has no
considerations" fixes it; an author reading "compile failed" files a bug.

**Set `m_watchedScopes` from what you actually read.** This is the reactivity switch. Mark a scope
you only *write* and every agent wakes on its own output — which, for a program whose steps write
what its own conditions read, is an infinite replan loop.

---

## Step 5 — Decide

```cpp
Decision Decide(const PlanContext& context, const AgentProgram& program, BrainState state,
                ActionResult lastResult, float elapsed, ActionPlan& outPlan) override
{
    auto& mine = *reinterpret_cast<MyState*>(state.data());

    const AZ::u16 best = ScoreEverything(context, program, mine);
    if (best == None)
    {
        // Nothing to do. Say how long before it is worth asking again.
        return Decision{ false, 0.5f };
    }

    AZStd::fixed_vector<ActionRequest, 16> steps;
    BuildSteps(best, steps);

    outPlan.m_span = context.m_planStore->Acquire(steps.data(), aznumeric_cast<AZ::u32>(steps.size()));
    return Decision{ true, 0.0f };
}
```

`Acquire` borrows a block from the [[PlanStore]] and the runtime gives it back when the plan ends
or is abandoned. Do not hold the span yourself.

Returning `m_planned = false` is a perfectly normal answer. `m_wakeIn` is how long before you are
worth asking again — return something sensible, not zero, or you will be asked every tick forever.

---

## Step 6 — Say when a plan is stale

```cpp
TickResult Advance(const PlanContext& context, const AgentProgram& program, BrainState state,
                   float elapsed, size_t runningStep) override
{
    if (StillTheBestChoice(context, state)) { return TickResult::Continue; }
    return TickResult::Abandon;
}
```

This is called only when a scope you watch changed, or when `m_wantsTick` is set.

`Abandon` drops the plan and you get asked to `Decide` again. That is the whole interruption
contract — you cannot end a step early or reach the state machine, by design.

> **The trap worth naming.** Do not re-check the conditions that *chose* the plan. Re-check the
> conditions of what has **not run yet**. A plan whose own steps write the variable its choice was
> made on will otherwise abandon itself forever. The HTN backend hit exactly this and validates
> only the remaining primitives.

---

## Step 7 — Register it

```cpp
void GOAT_MyParadigmSystemComponent::Activate()
{
    auto backend = AZStd::unique_ptr<GOAT::IDecisionBackend>(aznew MyParadigmBackend(...));

    bool registered = false;
    GOAT::GOATBackendRequestBus::BroadcastResult(
        registered, &GOAT::GOATBackendRequests::RegisterDecisionBackend, backend);

    AZ_Error("GOAT", registered, "The my_paradigm backend could not be registered");
}
```

The parameter is a reference to the `unique_ptr` so ownership survives the bus. Names are unique;
a taken one fails and says so.

Agents then pick it by putting `my_paradigm` in the **Brain** field.

---

## Step 8 — Test it

Unit tests need no entity and no level. `Code/Tests/Backends/HtnTests.cpp` is the pattern; the
minimum is:

- a program compiles, and a bad one fails with a useful message
- `Decide` produces the steps you expect
- `Advance` returns `Abandon` when it should
- `Advance` returns `Continue` when the agent's **own** step wrote the variable

That last one is the regression test for the trap in step 6. Write it first.

---

## Checklist

- [ ] Own gem, listed in the root `gem.json`, with an Editor variant
- [ ] `GetStateSize` matches what you actually store
- [ ] Words declared in your own Lua file, not `GOAT.lua`
- [ ] `Compile` fails with a reason
- [ ] `m_watchedScopes` set from what you **read**
- [ ] `Decide` returns a sensible `m_wakeIn` when it plans nothing
- [ ] `Advance` re-checks what has not run yet, not what chose the plan
- [ ] Tests, including the replan-on-own-effects one
- [ ] Your words do not already exist. `GOAT_DeclareNode` leaves an existing global alone rather
      than taking it over, so a word `GOAT.lua` already defines fails silently at author time.
      `option`, `plan`, `backend`, `behavior` and `flow` are taken.
- [ ] Your root declaration writes `GOAT._trees[name]`, or nothing can find the program
- [ ] No `delegate` in a plan step. The core reserves the name (`RegisterAction` asserts on it)
      because a plan step naming it would let a plan re-enter the tree that asked for it. A
      program reaches another paradigm from a plan step with `embed`.

---

## Related

- [[IDecisionBackend]]
- [[Backend Abstraction Theory]]
- [[Extensibility Model]]
- [[AgentProgram]]
- [[PlanStore]]
- [[Backends]]

---

*Last updated: 2026-08-29*
