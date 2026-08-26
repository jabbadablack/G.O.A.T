# GOAT_SmartObject

Lets entities advertise what they are for, so agents can find and use them. This is the Sims-style
smart object pattern: the object carries the knowledge, and the agent's tree stays generic.

Depends on GOAT alone — **not** on navigation, animation, or physics.

## What it adds

| Word | Takes | What it does |
|---|---|---|
| `claim_smart_object` | `use` (required), `radius` | Takes a slot on the nearest entity offering that use |
| `use_smart_object` | `seconds` (required) | Runs the claimed use, then gives the slot back |

Plus three agent-scoped blackboard variables, so no `.bbx` has to mention them:

| Variable | Type | Meaning |
|---|---|---|
| `so_entity` | EntityId | The entity whose slot this agent holds |
| `so_anchor` | Vector3 | Where to stand to use it, in world space |
| `so_use` | Name | Which use was claimed |

## Why claiming and using are two words

The agent has to travel to the object in between, and **travelling belongs to whatever gem the
project moves with**. Publishing `so_anchor` is what lets navigation — or a project's own character
controller — carry the agent there without this module depending on either:

```lua
sequence {
    claim_smart_object "sit" { radius = 15 },
    move_to "so_anchor" { tolerance = 0.5 },   -- from GOAT_Navigation, or your own controller
    use_smart_object { seconds = 5 },
}
```

The slot is held across all three steps. It is given back when `use_smart_object` ends — including
when the branch is **aborted**, since the release is in `End` rather than on success — when the
agent claims something else, or when the object itself goes away. An agent holds at most one claim,
which is what bounds a leaked slot to one per agent rather than one per failed attempt.

## Setting up an object

Add **GOAT Smart Object** to any entity with a transform:

| Field | Meaning |
|---|---|
| Uses | What an agent asks for, as in `sit`. One entity may offer several. |
| Anchor offset | Where the agent should stand, relative to the entity. It is a **local** offset, so it scales with the entity: on an object scaled to 0.6, a metre out is `-1.667`. |
| Capacity | How many agents may use it at once. |

The entity needs to know nothing about AI. Console variable `goat_smartObjectRadius` (default 20)
is how far a claim looks when a node names no radius.
