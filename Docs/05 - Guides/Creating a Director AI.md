---
type: guide
status: active
tags: [guide, tutorial, how-to]
---

# Creating a Director AI

> **Difficulty:** Intermediate  
> **Time to Complete:** 20 minutes  
> **Prerequisites:** [[Director AI]], [[Backends]], Basic Lua knowledge

---

## 🎯 Objective

Learn how to set up a **Director AI** to control global game flow (spawning waves, adjusting difficulty, reacting to player progress) using a Global Agent entity and a `delegate` node with a Director backend.

---

## 📋 Prerequisites

- [ ] **O3DE Environment:** Set up and ready to build.
- [ ] **G.O.A.T. Gem:** Activated in your project.
- **A .bbx Asset:** Declaring Global scope variables (e.g., `wave_number`, `game_state`).

---

## 🪜 Step-by-Step Instructions

### Step 1: Create the Director Entity

1. In the O3DE Editor, create a new entity and name it `"Director"`.
2. Add a `GOATAgentComponent` to it.
3. Set the **Band** to `3` (runs every 1000ms, saving CPU).
4. Set the **TreeName** to `"DirectorTree"` (which we'll define in Lua).
5. Add your `.bbx` asset to the **Blackboards** field to declare the Global variables.

---

### Step 2: Define the Director Backend (Lua)

Create a new Lua script (e.g., `Director.lua`) and define a backend that handles global directives.

```lua
-- Director.lua
backend "Director" {
    plan = function(me, ctx, goal)
        if goal == "SpawnWave" then
            local wave = ctx:GetInt("wave_number") or 1
            return {
                { action = "script", behavior = "SpawnEnemies", tag = "wave_" .. wave },
                { action = "wait", seconds = 5.0 },
            }
        end
        return nil -- Planning failed
    end,
}

behavior "SpawnEnemies" {
    tick = function(me, ctx)
        -- Spawn enemies (via a registered action)
        return SUCCESS
    end,
}
```

---

### Step 3: Define the Director Tree (Lua)

In the same script (or a separate one), define the global tree that uses the backend.

```lua
return tree "DirectorTree" {
    sequence {
        condition "game_state" { abort = "lower_priority" },
        delegate "Director" { goal = "SpawnWave" },
        wait(2.0),
    }
}
```

---

### Step 4: Load the Scripts

In the `GOATAgentComponent` on your Director entity, add the script (e.g., `Director.lua`) to the **Scripts** field.

---

### Step 5: Verify

1. **Console:** Run `goat_listBackends` and ensure `"Director"` is listed.
2. **Console:** Run `goat_listAgents` and see your Director entity running `DirectorTree`.
3. **Behavior:** When `game_state` changes, the Director should spawn enemies.

---

## ✅ Verification

1. **Build:** The project compiles without errors.
2. **Console:** `goat_listBackends` shows your backend.
3. **Behavior:** The Director reacts to Blackboard changes and executes its plan.

---

## 🆘 Troubleshooting

| Issue | Cause | Solution |
| :--- | :--- | :--- |
| Director not running | `TreeName` not set or invalid | Ensure the tree name matches exactly. |
| Backend not found | `delegate "Director"` references a non-existent backend | Ensure the backend is defined and loaded. |
| No global state changes | Conditions not reading the right Blackboard keys | Ensure the `.bbx` asset declares the correct Global variables. |

---

## 🔗 Related Guides

- [[Adding New Actions]]
- [[Writing Custom Backends]]
- [[Director AI]]
- [[Orchestration Patterns]]

---

*Last updated: 2026-08-26*