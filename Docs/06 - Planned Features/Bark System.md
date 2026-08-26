---
type: module
status: planned
tags: [planned, bark, module]
---

# Bark System

> **Status:** Planned  
> **Target Version:** Future  
> **Related Folder:** `Code/Source/Modules/Animation/` (Potential)  
> **Build Flag:** `GOAT_WITH_ANIMATION=ON`

---

## 🎯 Objective

The Bark System will provide **trigger-based social reactions** for NPCs. Instead of NPCs constantly checking for player proximity, dedicated **Trigger Volumes** will detect entity entry and send requests to NPCs' `BarkLogicComponent`.

This allows NPCs to react to events (like the player entering a room) without polling every frame, and keeps detection logic centralized in the world space rather than on every NPC.

---

## 🧠 Theoretical Approach

Following G.O.A.T.'s **Extensibility Model**, the Bark system will be split into two distinct parts:

1. **Trigger Volume (Detection):** A new component that detects entity entry/exit and sends a bark request via EBus.
2. **NPC Response Logic (BarkLogic):** A component or action that evaluates the request, checks cooldowns/priority, and plays audio/animations.

### Why a Component, Not Just a Library?
Unlike Navigation (which is just a set of verbs), Bark needs **event-driven detection** (collision triggers) and **visual/audio playback** (animations, sounds). This aligns better with standard O3DE components.

---

## 🗂️ Proposed File Structure

```text
Code/Source/Modules/Animation/
├── BarkLogicComponent.cpp
├── BarkLogicComponent.h
├── BarkDefinition.h
└── CMakeLists.txt
```

```text
Code/Source/Modules/SmartObject/
├── BarkTriggerComponent.cpp
├── BarkTriggerComponent.h
└── CMakeLists.txt
```

---

## 📜 Proposed Public API

### BarkLogicComponent (Attached to NPC)

```cpp
class BarkLogicComponent : public AZ::Component
{
public:
    AZ_COMPONENT(BarkLogicComponent, BarkLogicComponentTypeId);

    void OnBarkRequest(const BarkRequest& request);

private:
    AZStd::vector<BarkDefinition> m_barkDefinitions;
    AZStd::unordered_map<AZ::Name, float> m_cooldowns;
};
```

### BarkTriggerComponent (Attached to Trigger Volume)

```cpp
class BarkTriggerComponent : public AZ::Component
{
public:
    AZ_COMPONENT(BarkTriggerComponent, BarkTriggerComponentTypeId);

    void OnTriggerEnter(const AZ::EntityId& entity);

private:
    AZStd::vector<AZ::Tag> m_validTargetTags;
    float m_triggerCooldown = 1.0f;
};
```

---

## 🧪 Example Usage in Lua

Once implemented, the relationship will be set up in the Editor:

```lua
-- Define a bark behavior in Lua (if using the behavior system)
behavior "GreetPlayer" {
    tick = function(me, ctx)
        -- Play greeting animation/audio (via a registered action)
        return SUCCESS
    end,
}
```

The `BarkTriggerComponent` will send a request to `BarkLogicComponent` via EBus.

---

## 🗺️ Build Integration

The Bark System would be part of the `GOAT_WITH_ANIMATION` module, which auto-enables if the `EMotionFX` gem is present.

---

## ✅ Implementation Checklist

- [ ] Define `BarkDefinition` struct (ID, audio, priority, cooldown).
- [ ] Implement `BarkTriggerComponent` (collision detection, tag filtering, trigger cooldown).
- [ ] Implement `BarkLogicComponent` (cooldown management, priority selection, animation/audio playback).
- [ ] Expose both components to the Editor via `Reflect()`.
- [ ] Register "PlayBark" as a core `IActionState` so Lua behavior trees can trigger barks manually.
- [ ] Write unit tests for cooldown and priority logic.

---

## 🔗 Related Notes

- [[Design Principles]]
- [[Extensibility Model]]
- [[Adding New Actions]]
- [[Bark]]

---

*Last updated: 2026-08-26*