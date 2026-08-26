---
type: module
status: planned
tags: [module, navigation, missing]
---

# Navigation

> **Status:** Planned  
> **Folder:** `Code/Source/Modules/Navigation/`  
> **Current State:** Empty (No implementation exists yet)

---

## 🎯 Objective

The Navigation module will provide a **library of movement actions** for G.O.A.T. agents. Instead of being a separate component, navigation verbs (like `MoveTo`, `Wander`, `Flee`) will be registered as `IActionState`s that agents can call directly from Lua behavior trees.

---

## 🧠 Theoretical Approach

Following G.O.A.T.'s **Extensibility Model**, Navigation will not be a component. It will be a **library of `IActionState`s** registered via `IAgentSystem::RegisterAction`.

### Why not a component?
- **Performance:** A component would add unnecessary per-entity overhead. A library of verbs keeps the runtime lightweight.
- **Flexibility:** Any tree can use any navigation verb via `script "MoveTo"` or `raw "MoveTo"`, without needing a separate component on the entity.
- **Lua-First:** Designers can use navigation directly in their trees without setting up additional infrastructure.

---

## 🗂️ Proposed File Structure

```text
Code/Source/Modules/Navigation/
├── MoveToAction.cpp
├── MoveToAction.h
├── WanderAction.cpp
├── WanderAction.h
├── FleeAction.cpp
├── FleeAction.h
├── FollowPathAction.cpp
├── FollowPathAction.h
└── CMakeLists.txt
```

---

## 📜 Proposed Public API

Each navigation verb will implement `IActionState`:

```cpp
// Example: MoveToAction
class MoveToAction final : public IActionState
{
public:
    AZ_CLASS_ALLOCATOR(MoveToAction, AZ::SystemAllocator);

    AZ::Name GetName() const override { return AZ_NAME_LITERAL("MoveTo"); }

    ActionResult OnStart(const ActionRequest& request, const PlanContext& context) override;
    ActionResult OnTick(const ActionRequest& request, const PlanContext& context, float deltaTime) override;
    void OnStop(const ActionRequest& request, const PlanContext& context) override;
};
```

---

## 🗝️ Proposed Actions

| Action | Description | Blackboard Key |
| :--- | :--- | :--- |
| `MoveTo` | Moves to a target position or entity. | `Vector3` or `EntityId` |
| `Wander` | Moves randomly within a radius. | `Vector3` (center) |
| `Flee` | Moves away from a threat. | `EntityId` (threat) |
| `FollowPath` | Follows a predefined path. | `EntityIdList` (waypoints) |
| `Seek` | Pursues a moving target. | `EntityId` (target) |
| `Arrive` | Moves to a target, decelerating on approach. | `Vector3` (destination) |

---

## 🧪 Example Usage in Lua

Once implemented, designers will use it like this:

```lua
behavior "Patrol" {
    tick = function(me, ctx)
        ctx:SetVector3("target_position", AZ::Vector3(10, 0, 0))
        return SUCCESS
    end,
}

return tree "Agent" {
    sequence {
        script "Patrol",
        raw "MoveTo" { key = "target_position" },
        wait(1.0),
    }
}
```

---

## ✅ Implementation Checklist

- [ ] Define the `MoveToAction` class implementing `IActionState`.
- [ ] Integrate with O3DE's `NavigationSystem` (Recast/Detour or custom).
- [ ] Register `MoveToAction` via `IAgentSystem::RegisterAction`.
- [ ] Expose `MoveTo` to Lua via `BehaviorContext`.
- [ ] Add `WanderAction`, `FleeAction`, and other verbs.
- [ ] Write unit tests for each action.

---

## 🔗 Related Notes

- [[Design Principles]]
- [[Extensibility Model]]
- [[Adding New Actions]]
- [[Planned Features]]

---

*Last updated: 2026-08-26*