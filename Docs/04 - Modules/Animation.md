---
type: module
status: implemented
tags: [module, animation]
---

# Animation (GOAT_Animation)

> **Status:** Implemented
> **Gem:** `GOAT_Animation`
> **Folder:** `Modules/Animation/`

---

## What it is

Verbs for driving animation from an agent's program, so a behaviour can play a motion without the
core knowing anything about animation.

| Verb | Does |
| :--- | :--- |
| `play_motion` | plays a named motion |
| `animate` | drives animation state from the agent |

---

## Why it is a separate gem

Animation drags in EMotionFX. Keeping it out of the core means a project that renders its
characters some other way — or a headless server running the same AI — does not pay for it, or
even build it.

This is the same reasoning as the paradigm gems: the core holds what every game needs and nothing
else.

---

## Related

- [[Extensibility Model]]
- [[Adding New Actions]]
- [[Navigation]]

---

*Last updated: 2026-08-27*
