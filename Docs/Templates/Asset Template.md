---
type: asset
status: active
tags: [asset, data, schema]
---

# {{Title}}

> **Asset Type:** {{BlackboardAsset (`.bbx`) / Lua Script / JSON Schema}}  
> **Location:** `Assets/.../{{FileName}}`  

---

## 📌 Purpose

{{What this asset defines and how it is used in gameplay.}}

---

## 🗝️ Schema & Properties

### Variables / Fields

| Property | Type | Scope | Description | Default |
| :--- | :--- | :--- | :--- | :--- |
| `{{Key}}` | `{{Bool/Int/Float/Vector3/Name/EntityId}}` | `{{Global/Agent/Squad}}` | {{Description}} | `{{Default}}` |

---

## 🧪 Example

```lua
-- Example usage in a Lua script
return tree "ExampleAgent" {
    sequence {
        script "BehaviorA",
        wait(1.0),
        condition "TargetVisible" { abort = "lower_priority" },
        script "BehaviorB",
    }
}
```

---

## 🔗 Related Assets & Notes

- [[Blackboard System]]
- [[Behavior DSL]]
- [[GOATAgentComponent]]

---

## ⚠️ Constraints & Validation

- **Duplicate Names:** Names are shared across all `.bbx` assets. Duplicates will cause a load error.
- **Type Mismatch:** A tree referencing a variable with the wrong type will fail at compile time.

---

*Last updated: {{date}}*