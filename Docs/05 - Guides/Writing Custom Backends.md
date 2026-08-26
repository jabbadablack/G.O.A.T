---
type: guide
status: active
tags: [guide, tutorial, how-to]
---

# Writing Custom Backends

> **Difficulty:** Advanced  
> **Time to Complete:** 45 minutes  
> **Prerequisites:** [[IBackend]], [[Extensibility Model]], Basic C++ or Lua knowledge

---

## 🎯 Objective

Learn how to write a custom **planning backend** for G.O.A.T. Backends turn `Intent`s from tree leaves into `ActionPlan`s. This guide covers both **C++ backends** (for complex algorithms like GOAP or HTN) and **Lua backends** (for quick, designer-friendly planning logic).

---

## 📋 Prerequisites

- [ ] **O3DE Environment:** Set up and ready to build.
- [ ] **G.O.A.T. Gem:** Activated in your project.
- [ ] **Understanding of `IBackend`:** Read [[IBackend]] first.
- **For C++ Backends:** A module to place the code in.
- **For Lua Backends:** A basic Lua script where you'll define the backend.

---

## 🪜 Option A: Writing a C++ Backend

### Step 1: Create the Backend Class

Create a new C++ class that implements `IBackend`.

**Header (MyGoapBackend.h):**

```cpp
#pragma once

#include <GOAT/Interfaces/IBackend.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace MyModule
{
    class MyGoapBackend final : public GOAT::IBackend
    {
    public:
        AZ_CLASS_ALLOCATOR(MyGoapBackend, AZ::SystemAllocator);

        AZ::Name GetName() const override { return AZ_NAME_LITERAL("MyGoap"); }

        bool Plan(const GOAT::PlanContext& context, const GOAT::Intent& intent, GOAT::ActionPlan& outPlan) override;

        void CollectGuards(
            const GOAT::PlanContext& context,
            const GOAT::ActionPlan& plan,
            GOAT::GuardList& outGuards) const override;

        void Release(const GOAT::PlanContext& context) override;
    };
}
```

**Implementation (MyGoapBackend.cpp):**

```cpp
#include "MyGoapBackend.h"
#include <AzCore/Name/NameDictionary.h>

namespace MyModule
{
    bool MyGoapBackend::Plan(const GOAT::PlanContext& context, const GOAT::Intent& intent, GOAT::ActionPlan& outPlan)
    {
        // Implement your planning algorithm here.
        // Read goals, check preconditions, generate steps.
        // If planning fails, return false.

        outPlan.m_steps.clear();

        // Example: A simple plan for "DefeatEnemy"
        if (intent.m_goal == AZ_NAME_LITERAL("DefeatEnemy"))
        {
            GOAT::ActionRequest move;
            move.m_action = m_moveToActionId; // Resolve via ActionStateRegistry
            move.m_targetKey = context.m_blackboard->FindKey(AZ::Name("TargetPosition"));
            outPlan.m_steps.push_back(move);

            GOAT::ActionRequest attack;
            attack.m_action = m_attackActionId;
            outPlan.m_steps.push_back(attack);

            return true;
        }

        return false;
    }

    void MyGoapBackend::CollectGuards(const GOAT::PlanContext& context, const GOAT::ActionPlan& plan, GOAT::GuardList& outGuards) const
    {
        // Report conditions that invalidate the plan while running.
        // e.g., "Target is dead" or "Health is low"
    }

    void MyGoapBackend::Release(const GOAT::PlanContext& context)
    {
        // Clean up per-agent state if any.
    }
}
```

---

### Step 2: Register the Backend

To make the backend available to the system, you must register it with `IAgentSystem`. This is typically done in your module's system component `Activate()` method.

```cpp
// MyModuleSystemComponent.cpp
#include <GOAT/Interfaces/IAgentSystem.h>
#include "MyGoapBackend.h"

void MyModuleSystemComponent::Activate()
{
    if (auto* agentSystem = GOAT::AgentSystemInterface::Get())
    {
        agentSystem->RegisterBackend(AZStd::make_unique<MyGoapBackend>());
    }
}
```

---

### Step 3: Use it in Lua

Once registered, the backend can be used in any behavior tree via the `delegate` node.

```lua
delegate "MyGoap" { goal = "DefeatEnemy" }
```

---

## 🪜 Option B: Writing a Lua Backend

### Step 1: Define the Backend in Lua

Lua backends are simpler and can be defined in any script loaded by `GOATAgentComponent`.

```lua
-- MyLuaBackend.lua
backend "Errand" {
    plan = function(me, ctx, goal)
        if goal == "Rest" then
            return { { action = "wait", seconds = 2.0 } }
        end
        return {
            { action = "script", behavior = "Announce" },
            { action = "wait", seconds = 0.5 },
        }
    end,
}
```

### Step 2: Register the Script

Add the script to the **Scripts** field of your `GOATAgentComponent`. The system automatically detects `backend` declarations and registers them as `LuaBackend` instances.

### Step 3: Use it in Lua

```lua
delegate "Errand" { goal = "Deliver" }
```

---

## ✅ Verification

1. **Build:** The project compiles without errors.
2. **Console:** Run `goat_listBackends` to see your backend listed.
3. **Test:** Create a tree with `delegate "YourBackend"` and ensure the plan executes correctly.

---

## 🆘 Troubleshooting

| Issue | Cause | Solution |
| :--- | :--- | :--- |
| Backend not listed in `goat_listBackends` | Not registered correctly | Check your `Activate()` method and ensure `IAgentSystem::RegisterBackend` is called. |
| Lua error: `Unknown backend` | The script wasn't loaded or the name doesn't match | Ensure the script is loaded and the backend name matches exactly. |
| Plan returns `nil` | The backend couldn't plan for the given goal | Add a fallback plan or ensure the goal is handled. |

---

## 🔗 Related Guides

- [[Adding New Actions]]
- [[Creating a Director AI]]
- [[IBackend]]
- [[LuaBackend]]

---

*Last updated: 2026-08-26*