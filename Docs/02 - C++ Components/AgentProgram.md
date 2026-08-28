---
type: component
status: active
tags: [cpp, core, domain, backend]
---

# AgentProgram

> **Header:** `Code/Include/GOAT/Domain/AgentProgram.h`
> **Kind:** Base class, public API

---

## Overview

A compiled program a backend produced, **shared by every agent running it**. Ten thousand agents
of one kind hold one of these between them, not ten thousand copies.

It is the paradigm-neutral base. A backend subclasses it and puts whatever it needs inside:
[[DecisionProgram]] for a behaviour tree, `HtnDomain` for a task network. The core only ever
touches the fields here.

```cpp
class AgentProgram
{
public:
    AZ_RTTI(AgentProgram, AgentProgramTypeId);
    virtual ~AgentProgram() = default;

    AZ::Name m_name;
    IDecisionBackend* m_backend = nullptr;
    AZStd::array<bool, static_cast<size_t>(BlackboardScope::Count)> m_watchedScopes{};
    AZStd::vector<AZ::Name> m_boundSlots;
    bool m_wantsTick = false;
};
```

---

## The fields that matter

**`m_watchedScopes`** is the reactivity switch. It says which blackboard scopes this program
guards on, so a write anywhere else never wakes an agent running it. [[GuardWatch]] reads it when
an agent connects. A program that guards on nothing is never woken by a write at all.

**`m_wantsTick`** is the opt-out: true when the program needs a call every tick regardless, not
only when a watched slot changed. Services need this; a plain reactive tree does not.

**`m_boundSlots`** records which subtree slots the program was compiled against, deduplicated.
The compiler that resolved them is the only thing that knows, and rebinding a slot has to find
everyone who used it again.

**`m_backend`** points back at whoever compiled it. An agent with a program whose backend is null
is refused registration — that is what "it has no compiled program to run" means in the log.

---

## Lifetime

Held as `shared_ptr<const AgentProgram>` in an [[AgentArchetype]], which is what two identically
authored entities share. A program is immutable once compiled, so a rebind never rewrites one
under an agent mid-action; agents pick up the new one next time they enter that program.

---

## Related

- [[IDecisionBackend]]
- [[DecisionProgram]]
- [[AgentArchetype]]
- [[GuardWatch]]

---

*Last updated: 2026-08-27*
