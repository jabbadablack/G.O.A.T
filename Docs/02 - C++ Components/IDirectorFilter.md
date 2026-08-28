---
type: component
status: active
tags: [cpp, core, interface, director, extensibility]
---

# IDirectorFilter

> **Header:** `Code/Include/GOAT/Interfaces/IDirectorFilter.h`
> **Kind:** Extension interface, public API

---

## Overview

Narrows which agents one director governs.

A director is global on its own. A component beside it attaches one of these, and several
attached filters combine with AND. Narrowing is therefore **composed** rather than authored on
the director itself.

```cpp
class IDirectorFilter
{
public:
    AZ_RTTI(IDirectorFilter, IDirectorFilterTypeId);

    virtual ~IDirectorFilter() = default;

    //! True when this filter would let the director govern that agent.
    virtual bool Accepts(AgentId agent, AZ::EntityId entity) const = 0;
};
```

That is the whole interface. No positions, no range: the core stopped doing geometry when this
replaced the old named reach filters, so a filter that cares about distance measures it itself.

The entity is handed over with the agent because every filter needs it and
[[DirectorRegistry]] is already holding it.

---

## Attaching one

```cpp
virtual bool AttachDirectorFilter(AgentId director, IDirectorFilter& filter) = 0;
virtual void DetachDirectorFilter(AgentId director, IDirectorFilter& filter) = 0;
```

Both are on [[IAgentSystem]]. The filter is **not owned** — the usual pattern is that the
component *is* the filter, so it outlives the attachment and detaches itself in `Deactivate`.

Attaching or detaching throws away the director's cached reach, so a filter added mid-band
applies on the very next resolve rather than a tick later.

---

## Two rules

**Fail open.** A filter that cannot answer must return `true`. A broken setup should be one loud
warning, not a director that silently governs nobody — that is far harder to diagnose.

**Warn once, not per agent.** `Accepts` is called for every agent in the level on every director
tick. A warning inside it will flood the log unless you latch it.

---

## Writing one

```cpp
class MyFilterComponent
    : public AZ::Component
    , public IDirectorFilter
{
public:
    static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("GOATDirectorService"));
    }

    bool Accepts(AgentId agent, AZ::EntityId entity) const override
    {
        return /* your test */;
    }

protected:
    void Activate() override
    {
        auto* director = GetEntity()->FindComponent<GOATDirectorComponent>();
        m_director = director != nullptr ? director->GetAgentId() : AgentId{};
        if (m_director.IsNull()) { return; }

        if (auto* agents = AgentSystemInterface::Get())
        {
            agents->AttachDirectorFilter(m_director, *this);
        }
    }

    void Deactivate() override
    {
        if (auto* agents = AgentSystemInterface::Get(); agents && !m_director.IsNull())
        {
            agents->DetachDirectorFilter(m_director, *this);
        }
        m_director = AgentId{};
    }

private:
    AgentId m_director;
};
```

Requiring `GOATDirectorService` is what makes the ordering safe rather than lucky: O3DE activates
required services first, so `GetAgentId()` is already valid, and deactivates dependents first, so
the filter always detaches before the director goes.

---

## The two that ship

- [[GOATDirectorAreaFilterComponent]] — inside a shape
- [[GOATDirectorSquadFilterComponent]] — in a squad, or carrying a tag

---

## What this replaced

`IReachFilter`, a `ReachFilterRegistry` keyed by name, and the navigation gem's `path_distance`
and `ahead_of` filters. A director used to name one filter as a string; now it composes as many
components as it likes, and the core no longer keeps a registry of them.

---

## Related

- [[Director AI]]
- [[GOATDirectorComponent]]
- [[IAgentSystem]]
- [[Extensibility Model]]

---

*Last updated: 2026-08-27*
