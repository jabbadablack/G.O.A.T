# G.O.A.T

A pluggable NPC AI framework for O3DE. Behaviour trees and node logic are written in Lua; the C++
core supplies the blackboards, the state machine and the scheduling. Backends (behaviour tree,
HTN, GOAP, Bark, Director) plug in behind one interface, and blackboard variables authored as
`.bbx` assets are the only thing any two stages share.

See [Docs/README.md](Docs/README.md) to get an agent running, and
[Code/Source/Backends/README.md](Code/Source/Backends/README.md) for plans and backends.

## Gems

| Gem | Adds |
|---|---|
| `GOAT` | The core: trees, blackboards, the FSM, the Lua vocabulary |
| `GOAT_Navigation` | Movement and spatial checks, over RecastNavigation |
| `GOAT_SmartObject` | Objects that advertise what agents can do with them |
| `GOAT_Animation` | Anim graph parameters and motion playback, over EMotionFX |

The core is genre neutral: it names none of the modules and depends on none of them.
