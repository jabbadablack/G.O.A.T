---
type: architecture
status: implemented
tags: [architecture, core, pipeline]
---

# Data Flow

> **Category:** Architecture Pipeline  
> **Status:** Implemented  
> **Core Files:** `Code/Source/Core/Scripting/LuaDispatch.cpp`, `Code/Source/Core/Frontend/LuaTreeBuilder.cpp`, `Code/Source/Core/Frontend/TreeCompiler.cpp`, `Code/Source/Core/Frontend/TreeWalker.cpp`, `Code/Source/Core/Application/AgentRuntime.cpp`

---

## 💡 Core Concept

G.O.A.T. uses a **strict pipeline** to transform authored Lua trees into executable behavior. This pipeline is designed to be **one-way and predictable**: once a tree is compiled into a `DecisionProgram`, it is immutable and shared by all agents running it.

The flow is:

```text
Lua Authoring → LuaTreeBuilder → TreeCompiler → DecisionProgram → TreeWalker → Intent → Backend → ActionPlan → AgentRuntime → IActionState → Game World
```

---

## 🗺️ Visual Overview

```mermaid
graph LR
    L[Lua Script] -->|GOAT_EmitTree| D[LuaDispatch]
    D -->|BeginTree / AddNode| B[LuaTreeBuilder]
    B -->|BehaviorTreeNode| C[TreeCompiler]
    C -->|DecisionProgram| W[TreeWalker]
    W -->|Intent| K[BackendRegistry]
    K -->|ActionPlan| R[AgentRuntime]
    R -->|IActionState| A[Game World]
    A -->|Blackboard Updates| W
```

---

## 🧩 Detailed Pipeline

### Stage 1: Lua Authoring

Designers write trees in Lua using the DSL defined in `GOAT.lua`.

```lua
-- Example from ExampleAgent.lua
return tree "ExampleAgent" {
    selector {
        sequence {
            condition "target_seen" { abort = "lower_priority" },
            script "Alert",
            wait(1.0),
        },
        sequence {
            script "Patrol",
            wait(0.5),
        },
    },
}
```

**What happens:**

- `tree "ExampleAgent"` calls `GOAT.Compile(name, root)`.
- `GOAT.Compile` flattens the node graph into a pre-order record list.
- The compiled tree is stored in `GOAT._trees[name]`.

---

### Stage 2: Lua Dispatch to C++

`LuaDispatch::EmitTree` calls the Lua function `GOAT_EmitTree`, passing a `LuaTreeBuilder` object.

```cpp
// Code/Source/Core/Scripting/LuaDispatch.cpp
AZ::Outcome<AZStd::shared_ptr<const BehaviorTreeNode>, AZStd::string> LuaDispatch::EmitTree(const AZ::Name& treeName)
{
    AZ::ScriptDataContext call;
    if (!m_scriptContext->Call("GOAT_EmitTree", call))
    {
        return AZ::Failure(AZStd::string("The GOAT Lua vocabulary is not loaded"));
    }

    call.PushArg(AZStd::string(treeName.GetStringView()));
    call.PushArg(m_builder);

    if (!call.CallExecute())
    {
        return AZ::Failure(AZStd::string::format("Emitting tree '%s' raised a Lua error", treeName.GetCStr()));
    }

    bool found = false;
    if (call.GetNumResults() >= 1)
    {
        call.ReadResult(0, found);
    }

    if (!found)
    {
        return AZ::Failure(AZStd::string::format("No tree named '%s' was declared", treeName.GetCStr()));
    }

    if (!m_builder.IsComplete())
    {
        return AZ::Failure(AZStd::string::format(
            "Tree '%s' could not be assembled: %s", treeName.GetCStr(), m_builder.GetError().c_str()));
    }

    return AZ::Success(AZStd::shared_ptr<const BehaviorTreeNode>(aznew BehaviorTreeNode(m_builder.GetRoot())));
}
```

---

### Stage 3: LuaTreeBuilder Reconstructs the Hierarchy

`LuaTreeBuilder` receives flat node calls and reconstructs the nested `BehaviorTreeNode` structure.

```cpp
// Code/Source/Core/Scripting/LuaTreeBuilder.cpp
void LuaTreeBuilder::AddNode(AZStd::string type, int childCount, int serviceCount)
{
    Record record;
    record.m_type = AZStd::move(type);
    record.m_childCount = childCount;
    record.m_serviceCount = serviceCount;
    m_records.push_back(AZStd::move(record));
}
```

```cpp
// Code/Source/Core/Scripting/LuaTreeBuilder.cpp
size_t LuaTreeBuilder::Build(size_t index, BehaviorTreeNode& out)
{
    const Record& record = m_records[index++];
    out.m_type = record.m_type;
    out.m_properties = record.m_properties;

    for (int i = 0; i < record.m_serviceCount && m_error.empty(); ++i)
    {
        BehaviorTreeNode service;
        index = Build(index, service);
        out.m_services.push_back(AZStd::move(service));
    }

    for (int i = 0; i < record.m_childCount && m_error.empty(); ++i)
    {
        BehaviorTreeNode child;
        index = Build(index, child);
        out.m_children.push_back(AZStd::move(child));
    }

    return index;
}
```

---

### Stage 4: TreeCompiler Flattens the Tree

`TreeCompiler` validates the authored tree and flattens it into a `DecisionProgram`.

```cpp
// Code/Source/Core/Frontend/TreeCompiler.cpp
AZ::Outcome<NodeIndex, AZStd::string> TreeCompiler::Emit(
    const BehaviorTreeNode& authored,
    NodeIndex parent,
    AZ::u32 depth,
    DecisionProgram& program,
    AZStd::vector<AZ::Name>& inlining) const
{
    if (depth >= MaxTreeDepth)
    {
        return AZ::Failure(AZStd::string::format("Tree is deeper than the %zu node limit", MaxTreeDepth));
    }

    const AZ::Name typeName(authored.m_type);
    const NodeTypeDescriptor* descriptor = m_types.Find(typeName);
    if (descriptor == nullptr)
    {
        return AZ::Failure(AZStd::string::format("Unknown node type '%s'", authored.m_type.c_str()));
    }

    if (auto valid = Validate(authored, *descriptor); !valid.IsSuccess())
    {
        return AZ::Failure(valid.TakeError());
    }

    const NodeIndex index = aznumeric_cast<NodeIndex>(program.m_nodes.size());
    program.m_nodes.emplace_back();
    {
        DecisionNode& node = program.m_nodes[index];
        node.m_op = descriptor->m_op;
        node.m_parent = parent;
        node.m_childCount = aznumeric_cast<AZ::u16>(authored.m_children.size());
    }

    // Resolve properties, blackboard keys, etc.
    // ...

    // Emit children
    const NodeIndex firstChild = aznumeric_cast<NodeIndex>(program.m_nodes.size());
    for (const BehaviorTreeNode& child : authored.m_children)
    {
        auto emitted = Emit(child, index, depth + 1, program, inlining);
        if (!emitted.IsSuccess())
        {
            return emitted;
        }
    }

    DecisionNode& node = program.m_nodes[index];
    node.m_firstChild = authored.m_children.empty() ? InvalidNodeIndex : firstChild;
    node.m_subtreeEnd = aznumeric_cast<NodeIndex>(program.m_nodes.size());
    return AZ::Success(index);
}
```

---

### Stage 5: TreeWalker Executes the Program

`TreeWalker` iteratively traverses the `DecisionProgram`, producing `Intent`s for leaf nodes.

```cpp
// Code/Source/Core/Frontend/TreeWalker.cpp
WalkStep TreeWalker::Run(
    const DecisionProgram& program,
    DecisionCursor& cursor,
    const PlanContext& context,
    NodeIndex node,
    bool bubbling,
    ActionResult result) const
{
    while (node != InvalidNodeIndex)
    {
        if (!bubbling)
        {
            const DecisionNode& current = program.m_nodes[node];
            switch (current.m_op)
            {
            case NodeOp::Selector:
            case NodeOp::Sequence:
                cursor.ChildIndex(node) = 0;
                node = current.m_firstChild;
                continue;

            case NodeOp::Action:
            case NodeOp::Script:
            case NodeOp::Delegate:
                cursor.SetActiveLeaf(node);
                return Emitted(MakeIntent(current, node));

            // ... other node types ...
            }
        }

        // Bubbling logic
        // ...
    }

    return Finished(result);
}
```

---

### Stage 6: Backend Produces ActionPlan

When a `delegate` node is encountered, `TreeWalker` creates an `Intent` with the backend name and goal. The `BackendRegistry` finds the backend and calls `IBackend::Plan`.

```cpp
// Code/Source/Core/Frontend/DirectBackend.cpp
bool DirectBackend::Plan(
    [[maybe_unused]] const PlanContext& context, const Intent& intent, ActionPlan& outPlan)
{
    if (intent.m_direct.m_action == CoreActions::Invalid)
    {
        return false;
    }

    outPlan.m_steps.clear();
    outPlan.m_steps.push_back(intent.m_direct);
    return true;
}
```

For Lua backends:

```lua
-- Example from ExampleAdvanced.lua
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

---

### Stage 7: AgentRuntime Executes Actions

`AgentRuntime` processes the `ActionPlan`, calling `IActionState::OnStart`, `OnTick`, and `OnStop` for each step.

```cpp
// Code/Source/Core/Application/AgentRuntime.cpp
ActionResult AgentRuntime::ExecutePlan(const ActionPlan& plan, const PlanContext& context)
{
    for (const ActionRequest& request : plan.m_steps)
    {
        IActionState* action = m_actions->Find(request.m_action);
        if (action == nullptr)
        {
            return ActionResult::Failure;
        }

        ActionResult result = action->OnStart(request, context);
        if (result == ActionResult::Running)
        {
            // Store active action, tick until complete
            m_activeAction = request;
            m_activeResult = result;
            return ActionResult::Running;
        }

        if (result == ActionResult::Failure)
        {
            return ActionResult::Failure;
        }
    }

    return ActionResult::Success;
}
```

---

## ⚖️ Trade-offs

| ✅ Advantage | ⚠️ Disadvantage |
| :--- | :--- |
| One-way data flow is easy to reason about | Requires understanding the full pipeline |
| Compiled program is immutable and shared | Errors can be hard to trace if not logged |
| Type-safe blackboard access | Schema must be declared before compile |
| Modular backends allow paradigm swapping | Backend can fail silently if not handled |

---

## 🧩 Impact on the Codebase

### Lua Layer
- Trees are authored in Lua, compiled once, shared by many agents.
- Backends can be written in Lua or C++.

### C++ Core
- `TreeCompiler` and `TreeWalker` are the "brain" of the pipeline.
- `AgentRuntime` executes plans and manages active actions.

### Extensibility
- New actions (`IActionState`) plug into the final stage.
- New backends (`IBackend`) plug into the planning stage.

---

## 🔗 Related Notes

- [[Layered Overview]]
- [[Blackboard System]]
- [[TreeCompiler]]
- [[TreeWalker]]
- [[AgentRuntime]]

---

*Last updated: 2026-08-26*