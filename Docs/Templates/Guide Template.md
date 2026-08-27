---
type: guide
status: active
tags: [guide, tutorial, how-to]
---

# {{Title}}

> **Difficulty:** {{Beginner / Intermediate / Advanced}}  
> **Time to Complete:** {{Estimated time}}  
> **Prerequisites:** [[List any required notes or code knowledge]]

---

## 🎯 Objective

{{What the reader will accomplish by completing this guide.}}

---

## 📋 Prerequisites

- [ ] **O3DE Environment:** Set up and ready to build.
- [ ] **G.O.A.T. Gem:** Activated in your project.
- **Required Knowledge:** {{Basic C++ or Lua}}.

---

## 🪜 Step-by-Step Instructions

### Step 1: {{Task Title}}

{{Explain the context.}}

```cpp
// Code example here
void ExampleFunction() {
    // Implementation
}
```

---

### Step 2: {{Task Title}}

{{Explain what to do.}}

```lua
-- Lua example
behavior "ExampleBehavior" {
    tick = function(me, ctx)
        ctx:SetBool("done", true)
        return SUCCESS
    end
}
```

---

### Step 3: {{Task Title}}

{{Explain the final step.}}

---

## ✅ Verification

Run the following checks to confirm success:

1. **Build:** The project compiles without errors.
2. **Console:** `goat_listAgents` shows your agent running.
3. **Behavior:** The agent performs the expected action.

---

## 🆘 Troubleshooting

| Issue | Cause | Solution |
| :--- | :--- | :--- |
| {{Issue}} | {{Root cause}} | {{How to fix}} |

---

## 🔗 Related Guides

- [[Related Guide 1]]
- [[Related Guide 2]]

---

*Last updated: {{date}}*