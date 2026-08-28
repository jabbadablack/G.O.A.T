---
type: component
status: active
tags: [cpp, core, domain, director]
---

# DirectorProfile

> **Header:** `Code/Include/GOAT/Domain/DirectorProfile.h`
> **Kind:** Plain struct, public API

---

## Overview

What a director governs **with** — not who it governs. Two fields:

```cpp
struct DirectorProfile final
{
    AZ_TYPE_INFO(DirectorProfile, DirectorProfileTypeId);

    //! Higher outranks lower when two directors command the same agent in one window.
    AZ::u8 m_priority = 1;

    //! How long before this director may command the same agent the same way again.
    float m_cooldownSeconds = 5.0f;
};
```

The header also defines:

```cpp
//! What an agent switching its own tree carries.
inline constexpr AZ::u8 SelfSwitchPriority = 0;
```

Every director defaults above it, so any director outranks an agent switching itself. That is the
intended default: a director exists to overrule what an agent decided for itself.

---

## Why the cooldown lives here

It is held per director rather than per agent, keyed by agent *and* verb inside
[[DirectorRegistry]]. If it were held on the agent, one director's order would silence another's
by accident rather than by priority.

It only starts when a command actually changed something, so a no-op neither consumes nor starts
one.

---

## What used to be here

`DirectorReach` — squad, tree, radius and a named filter — was a member of this struct. It is
gone. Narrowing is now composed from filter components; see [[IDirectorFilter]].

---

## Related

- [[Director AI]]
- [[GOATDirectorComponent]]
- [[IDirectorFilter]]

---

*Last updated: 2026-08-27*
