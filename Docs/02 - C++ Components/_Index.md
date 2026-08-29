---
type: index
status: active
tags: [cpp, core, moc]
---

# C++ Components – Map of Content

This folder documents the **core C++ classes** that make up G.O.A.T. Each note covers a specific component, its responsibilities, interfaces, and dependencies.

---

## 📂 Notes in this Folder

### Interfaces
| Note | Description |
| :--- | :--- |
| [[IActionState]] | Interface for atomic action verbs (`Begin`, `Step`, `End`). |
| [[IBackend]] | Interface for a Lua-authored planner behind a `delegate`. |
| [[IDecisionBackend]] | Interface a whole paradigm implements. One per Brain. |
| [[IAgentSystem]] | Public API for modules to extend the framework. |
| [[IDirectorFilter]] | Narrows which agents a director governs. |
| [[IBlackboardSystem]] | Interface for the shared blackboard data. |
| [[INodeScripting]] | Interface for custom control flow routing. |

### Domain Types
| Note | Description |
| :--- | :--- |
| [[ActionState]] | Core enums and structs (`ActionResult`, `ActionRequest`, `CoreActions`). |
| [[ActionPlan]] | A span of `ActionRequest` steps in the [[PlanStore]]. |
| [[PlanStore]] | Where the steps of every plan live. |
| [[PlanContext]] | Everything a backend may reach while planning. |
| [[AgentProgram]] | A compiled program, shared by every agent running it. |
| [[ActionContext]] | Per-agent context passed to `IActionState`. |
| [[Intent]] | Message from a tree leaf to a backend. |
| [[DirectorProfile]] | A director's priority and cooldown. |
| [[DecisionProgram]] | An `AgentProgram` for a behaviour tree. Lives in the GOAT_BehaviorTree gem. |
| [[DecisionNode]] | One compiled node in a `DecisionProgram`. |
| [[DecisionService]] | Compiled representation of a service. |
| [[DecisionCursor]] | Per-agent execution state inside a tree. |
| [[Guard]] | Abort condition and mode for tree branches. |
| [[AbortMode]] | Enum for what a guard interrupts. |
| [[NodeType]] | Metadata for a node type (Kind, Op, Parameters). |
| [[NodeKind]] | Structural classification of a node. |
| [[NodeOp]] | Runtime operation of a compiled node. |
| [[NodeParameter]] | One property a node type accepts. |
| [[NodeTypeDescriptor]] | Complete descriptor for a node type. |
| [[Handle]] | Generation-checked reference. |
| [[AgentStore]] | Every agent, in slots whose index never moves. |
| [[AgentId]] | Type-safe handle for an agent. |
| [[AgentRecord]] | Per-agent runtime state. |
| [[AgentStateMachine]] | Executes `ActionPlan`s. |
| [[GuardWatch]] | Notices a watched scope changed, without subscribing. |
| [[GuardEvaluator]] | Re-checks guards when keys change. |
| [[ServiceTracker]] | Collects due services for a tick. |
| [[NodePredicate]] | Evaluates conditions/comparisons. |
| [[BlackboardKey]] | Typed index for a blackboard slot. |
| [[BlackboardLayout]] | Slot counts and defaults for a scope. |
| [[BlackboardSchema]] | Global namespace for variables. |
| [[BlackboardStorage]] | Typed arrays for a scope. |
| [[BlackboardTraits]] | C++ type to `BlackboardType` mapping. |
| [[BlackboardTypes]] | Enums and helpers for the blackboard. |
| [[BlackboardScope]] | Enum for where storage lives. |
| [[BlackboardVariable]] | One variable declared in a `.bbx` asset. |

### Assets
| Note | Description |
| :--- | :--- |
| [[BlackboardAsset]] | Authorable `.bbx` asset type. |
| [[BlackboardAssetHandler]] | Handler for `.bbx` assets. |
| [[ProgramAsset]] | Authorable `.goat` asset type. |
| [[AuthoredNode]] | One node exactly as it was authored. |
| [[AuthoredProperty]] | One property written on an authored node. |
| [[AuthoredNodeMetadata]] | Editor-only data a graph tool round-trips. |

### Registries & Runtime
| Note | Description |
| :--- | :--- |
| [[ActionStateRegistry]] | Stores and dispatches `IActionState`s. |
| [[BackendRegistry]] | Stores and dispatches `IBackend`s. |
| [[DecisionBackendRegistry]] | Stores and dispatches `IDecisionBackend`s. |
| [[AgentStore]] | Slot storage backing the agent registry. |
| [[AgentArchetype]] | The set of programs one kind of agent may run. |
| [[NodeTypeRegistry]] | Stores node type descriptors. |
| [[TreeLibrary]] | Stores authored trees and bindings. |
| [[AgentRegistry]] | Owns agents and schedules them into bands. |
| [[AgentRuntime]] | Runs one tick of the pipeline for an agent. |
| [[DirectorRegistry]] | Which directors exist and who each one governs. |

### Components & Modules
| Note | Description |
| :--- | :--- |
| [[GOATAgentComponent]] | Turns an entity into an AI agent. |
| [[GOATDirectorComponent]] | Turns an entity into a director. |
| [[GOATDirectorAreaFilterComponent]] | Narrows a director to a shape. |
| [[GOATDirectorSquadFilterComponent]] | Narrows a director to squads or tags. |
| [[GOATSystemComponent]] | "God object" owning all services. |
| [[GOATModuleInterface]] | Base module class. |
| [[GOATModule]] | Client module. |
| [[GOATEditorModule]] | Editor module. |
| [[GOATEditorSystemComponent]] | Editor-specific system component. |
| [[GOATBuilderComponent]] | Asset Processor builder component. |

### Actions
| Note | Description |
| :--- | :--- |
| [[WaitAction]] | Core action that waits for a duration. |
| [[RunScriptAction]] | Core action that runs a Lua behavior. |

### Scripting
| Note | Description |
| :--- | :--- |
| [[LuaDispatch]] | Bridge between C++ and Lua. |
| [[LuaBackend]] | Bridges a Lua backend to `IBackend`. |
| [[LuaTreeBuilder]] | Assembles authored trees from Lua. |
| [[LuaPlanBuilder]] | Assembles `ActionPlan`s from Lua backends. |
| [[LuaNodeScripting]] | Routes custom flow logic to Lua. |
| [[LuaNameCollector]] | Collects names from Lua. |
| [[AgentScriptContext]] | The `ctx` object exposed to Lua. |

---

## 🔗 Where to go next

- **Architecture:** Read [[Layered Overview]] to see how these components fit into the whole.
- **Lua API:** Read [[Vocabulary]] to see how designers interact with these systems.
- **Guides:** Read [[Writing Custom Backends]] to learn how to extend these components.

---

## 📌 How to use this folder

- Each note follows the [[Component Template]].
- Use `#cpp` and `#core` tags for filtering in Graph View.
- Link between component notes using `[[Wikilinks]]`.

---

*Last updated: 2026-08-26*