---
type: component
status: active
tags: [cpp, core, debugging, authoring]
---

# ProgramNodeRef

> **Header:** `Code/Include/GOAT/Domain/AgentDebug.h`
> **Kind:** Plain struct, public API

---

## Overview

One node of an authored program, named so that the tool which drew it can find it again.

```cpp
struct ProgramNodeRef final
{
    AZ::Name m_program;
    AZStd::vector<AZ::u16> m_path;
};
```

It is deliberately **authored**, not compiled. A backend's own node index means nothing outside
that backend, and handing one to the core would make the core know what a node is. A path over
the [[AuthoredNode]] tree is something the graph editor, the validator and every compiler can
all read.

---

## What a step means

`m_path` is a list of steps from the program's root. At each node, **services come first, then
children, sharing one index space** — the order `StepInto` walks:

```cpp
inline const AuthoredNode* StepInto(const AuthoredNode& node, size_t index);
```

`StepInto` exists so this is defined once. The validator writes a path with it, the canvas reads
one with it, and each compiler records one with it. When they disagree about what "the third
one" means, they each point at a different node — which is exactly the bug that fell out of
services and children starting from zero separately.

An empty path is the root.

---

## Why it carries a program name

Because a node is not always in the program that is running.

`TreeCompiler::Inline` expands a `subtree` reference in place: *"The subtree node itself leaves
no trace: its referenced root takes its place."* The nodes spliced in were authored in a
different tree, and the `subtree` node the author wrote compiles to nothing at all.

So `m_program` names the tree the node was actually written in. A tool comparing it against what
is on screen can tell that a glowing node belongs to a subtree it is not showing, and say so
rather than lighting whichever unrelated node happens to sit at that index.

---

## Where it is recorded

Each backend keeps a side table beside its compiled form, filled as it compiles, indexed the
same way its own nodes are:

| Backend | Table |
| :--- | :--- |
| `tree` | `DecisionProgram::m_authored`, one per `DecisionNode` |
| `htn` | `HtnDomain::m_authored`, one per `HtnTask` |
| `utility` | `UtilityProgram::m_authored`, one per `UtilityChoice` |

A side table rather than a field on the node itself, so the node stays the size its walker wants
— the same reason `m_guardNodes` and `m_serviceNodes` are separate. It costs once per compiled
program, which every agent running that program shares.

---

## Related Notes

- [[AgentSnapshot]]
- [[AuthoredNode]]
- [[DecisionProgram]]
- [[TreeCompiler]]
