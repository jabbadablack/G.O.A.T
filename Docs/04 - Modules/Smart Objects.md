---
type: module
status: implemented
tags: [module, smart-object]
---

# Smart Objects (GOAT_SmartObject)

> **Status:** Implemented
> **Gem:** `GOAT_SmartObject`
> **Folder:** `Modules/SmartObject/`

---

## What it is

Lets props advertise what they can be used for, so an agent can look for "somewhere to sit"
rather than being told about a specific bench.

Put a **GOAT Smart Object** component on the prop, list what it offers, and agents can claim it.

---

## Authoring the prop

| Field | What it is |
| :--- | :--- |
| `Uses` | what an agent asks for, as in `sit` or `drink`. A program claims one by name. |
| `Anchor offset` | where the agent should stand, relative to this entity |
| `Capacity` | how many agents may use it at once |

It requires a `TransformService`, because the anchor is this entity's transform plus the offset.

---

## The verbs

| Verb | Does |
| :--- | :--- |
| `claim_smart_object` | finds and reserves an object offering a use |
| `use_smart_object` | uses one already claimed |

A claim publishes three variables the rest of your program can read:

| Variable | Holds |
| :--- | :--- |
| `so_entity` | the object claimed |
| `so_anchor` | where to stand — feed this straight to `move_to` |
| `so_use` | which use was claimed |

---

## A typical shape

```lua
sequence {
    claim_smart_object "sit",
    move_to { key = "so_anchor", tolerance = 0.5 },
    use_smart_object "sit",
}
```

Claim, walk to the anchor, use it. Because `so_anchor` is an ordinary blackboard variable, the
navigation gem needs to know nothing about smart objects.

---

## Capacity and claims

Capacity is what stops six agents converging on one chair. A claim is held until the agent
releases it or is unregistered, so an agent that dies mid-walk does not leave the bench reserved
forever.

---

## Related

- [[Navigation]]
- [[Extensibility Model]]
- [[Adding New Actions]]

---

*Last updated: 2026-08-27*
