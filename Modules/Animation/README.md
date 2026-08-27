# GOAT_Animation

Animation vocabulary for GOAT agents. Only this gem knows EMotionFX exists; a project that
animates some other way, or not at all, does not enable it and the words disappear with it.

## What it adds

| Word | Takes | What it does |
|---|---|---|
| `animate` | `parameter` (required), `key`, `amount` | Writes one named anim graph parameter, then succeeds |
| `play_motion` | `motion`, `seconds` | Plays a motion and runs for its duration |

### `animate` — the one to reach for

```lua
animate "Speed"   { key = "nav_remaining" }   -- value comes from the blackboard
animate "Alerted" { key = "target_seen" }
animate "Stance"  { amount = 2 }              -- or from the node itself
```

The tree says what the agent **is**; the anim graph decides what to play. No clip name ever
appears in a behaviour tree, so re-authoring the graph does not re-author the behaviour — the
same split Unreal has between a Behavior Tree and an Animation Blueprint.

Which parameter setter runs follows the blackboard variable's **declared type**: a `Bool` variable
goes to `SetNamedParameterBool`, `Float` and `Int` to the float setter, `Vector3` to the vector
setter. A tree therefore cannot silently push a bool into a float parameter and get a zero.
Needs an **Anim Graph** component on the agent.

### `play_motion` — for a project with no anim graph

```lua
play_motion "animations/wave.motion" { seconds = 2 }
play_motion {}                                  -- plays whatever the component already holds
```

Needs a **Simple Motion** component on the agent. Without a `seconds` the action runs for the
motion's own duration; with one, the tree can cut a long clip short.

## Setting up an agent

1. **Actor** component with the agent's actor asset.
2. **Anim Graph** component (for `animate`) or **Simple Motion** component (for `play_motion`).
3. **GOAT Agent**, pointed at the tree that uses these words.
