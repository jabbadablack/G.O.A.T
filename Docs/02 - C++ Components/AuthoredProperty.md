---
type: component
status: active
tags: [cpp, core, asset, authoring]
---

# AuthoredProperty

> **Header:** `Code/Include/GOAT/Assets/BehaviorTreeAsset.h`
> **Kind:** Plain struct, public API

---

## Overview

One property written on a node, kept as a name and an untyped value:

```cpp
struct AuthoredProperty final
{
    AZStd::string m_name;
    AZStd::any m_value;
};
```

It stays untyped until compile time, when it is checked against the node type's declared
[[NodeParameter]] list. That is what lets a misspelled property or a string where a number
belongs fail the compile with a useful message, instead of at runtime with none.

The `m_value` is an `AZStd::any` because a property can be a number, a string, a bool or a
blackboard key, and the authoring layer has no reason to know which until the node type says.

---

## Related

- [[AuthoredNode]]
- [[NodeParameter]]
- [[NodeTypeDescriptor]]

---

*Last updated: 2026-08-27*
