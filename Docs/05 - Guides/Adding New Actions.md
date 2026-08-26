---
type: guide
status: active
tags: [guide, tutorial, how-to]
---

# Adding New Actions

> **Difficulty:** Intermediate  
> **Time to Complete:** 30 minutes  
> **Prerequisites:** [[IBackend]], [[Extensibility Model]], Basic C++ knowledge

---

## 🎯 Objective

Learn how to create a custom `IActionState` (a "verb" like `MoveTo`, `Attack`, `PlayAnimation`) and register it with the `IAgentSystem` so it can be used in Lua trees via `raw` or `script`.

---

## 📋 Prerequisites

- [ ] **O3DE Environment:** Set up and ready to build.
- [ ] **G.O.A.T. Gem:** Activated in your project.
- **C++ Knowledge:** You must be able to create a new class and register it with an interface.

---

## 🪜 Step-by-Step Instructions

### Step 1: Create the Action Class

Create a new C++ class that implements `IActionState`. Place it in your module's `Code/Source` directory (e.g., `Code/Source/Modules/Navigation/MoveToAction.h` and `.cpp`).

**Header (MoveToAction.h):**

```cpp
#pragma once

#include <GOAT/Interfaces/IActionState.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace MyModule
{
    class MoveToAction final : public GOAT::IActionState
    {
    public:
        AZ_CLASS_ALLOCATOR(MoveToAction, AZ::SystemAllocator);

        AZ::Name GetName() const override { return AZ_NAME_LITERAL("MoveTo"); }

        GOAT::ActionResult OnStart(const GOAT::ActionRequest& request, const GOAT::PlanContext& context) override;
        GOAT::ActionResult OnTick(const GOAT::ActionRequest& request, const GOAT::PlanContext& context, float deltaTime) override;
        void OnStop(const GOAT::ActionRequest& request, const GOAT::PlanContext& context) override;
    };
}
```

**Implementation (MoveToAction.cpp):**

```cpp
#include "MoveToAction.h"
#include <AzCore/Component/TransformBus.h>

namespace MyModule
{
    GOAT::ActionResult MoveToAction::OnStart(const GOAT::ActionRequest& request, const GOAT::PlanContext& context)
    {
        // Read target from blackboard using request.m_targetKey
        auto target = context.m_blackboard->GetValue(request.m_targetKey);
        // Start movement logic here
        return GOAT::ActionResult::Running;
    }

    GOAT::ActionResult MoveToAction::OnTick(const GOAT::ActionRequest& request, const GOAT::PlanContext& context, float deltaTime)
    {
        // Update movement, check if arrived
        return GOAT::ActionResult::Success;
    }

    void MoveToAction::OnStop(const GOAT::ActionRequest& request, const GOAT::PlanContext& context)
    {
        // Cancel movement
    }
}
```

---

### Step 2: Register the Action

To make the action available to the system, you must register it with `IAgentSystem`. This is typically done in your module's system component `Activate()` method.

```cpp
// MyModuleSystemComponent.cpp
#include <GOAT/Interfaces/IAgentSystem.h>
#include "MoveToAction.h"

void MyModuleSystemComponent::Activate()
{
    if (auto* agentSystem = GOAT::AgentSystemInterface::Get())
    {
        agentSystem->RegisterAction(AZStd::make_unique<MoveToAction>());
    }
}
```

---

### Step 3: Use it in Lua

Once registered, the action can be used in any Lua behavior tree via the `raw` node (for direct verbs) or `script` (if you wrap it in a behavior).

**Directly via `raw`:**

```lua
raw "MoveTo" { key = "target_position" }
```

**Or wrapped in a behavior:**

```lua
behavior "GoToTarget" {
    tick = function(me, ctx)
        ctx:SetVector3("target_position", AZ::Vector3(10, 0, 0))
        return SUCCESS
    end,
}

-- In the tree
sequence {
    script "GoToTarget",
    raw "MoveTo" { key = "target_position" },
}
```

---

## ✅ Verification

1. **Build:** Your project compiles without errors.
2. **Console:** Run `goat_listActions` in the console. You should see `MoveTo` listed.
3. **Test:** Create a simple tree with `raw "MoveTo"` and ensure the agent moves.

---

## 🆘 Troubleshooting

| Issue | Cause | Solution |
| :--- | :--- | :--- |
| `MoveTo` not listed in `goat_listActions` | Not registered correctly | Check your `Activate()` method and ensure `IAgentSystem::RegisterAction` is called. |
| Lua error: `Unknown verb 'MoveTo'` | Action not registered or name mismatch | Ensure the `GetName()` returns exactly `"MoveTo"` (case-sensitive). |
| Action doesn't execute | The `key` points to an undeclared blackboard variable | Declare the variable in your `.bbx` asset or the schema. |

---

## 🔗 Related Guides

- [[Writing Custom Backends]]
- [[Creating a Director AI]]
- [[IBackend]]

---

*Last updated: 2026-08-26*