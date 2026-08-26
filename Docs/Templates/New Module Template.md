---
type: module
status: planned
tags: [module, roadmap]
---

# {{Module Name}}

> **Status:** {{Planned / In Progress / Implemented}}  
> **Target Version:** {{Version}}  
> **Build Flag:** `GOAT_WITH_???=ON` (if applicable)

---

## 🎯 Objective

{{What this module provides to the framework. For example, "A library of movement actions for agents, exposed via `IActionState`."}}

---

## 🧠 Theoretical Approach

{{Describe how this module fits into the backend-driven architecture. For example, "Navigation will be a set of `IActionState` implementations registered via `IAgentSystem::RegisterAction`. Actions like `MoveTo` and `Wander` will read target positions from the Blackboard and drive the entity's transform."}}

---

## 🗂️ Proposed File Structure

```text
Code/Source/Modules/{{ModuleName}}/
├── {{Class1}}.cpp
├── {{Class1}}.h
├── {{Class2}}.cpp
├── {{Class2}}.h
└── CMakeLists.txt
```

---

## 📜 Proposed Public API

```cpp
// Example interface
class I{{ModuleName}} {
public:
    virtual void MoveTo(const AZ::EntityId& entity, const AZ::Vector3& target) = 0;
};
```

---

## ✅ Implementation Checklist

- [ ] Define the public interface.
- [ ] Implement core logic.
- [ ] Register actions with `IAgentSystem::RegisterAction` or backends with `IAgentSystem::RegisterBackend`.
- [ ] Expose methods to Lua via `BehaviorContext`.
- [ ] Write tests.
- [ ] Update documentation.

---

## 🔗 Related Notes

- [[Design Principles]]
- [[Extensibility Model]]
- [[Adding New Actions]]

---

*Last updated: {{date}}*