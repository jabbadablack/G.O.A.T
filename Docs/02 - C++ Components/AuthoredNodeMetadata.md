---
type: component
status: active
tags: [cpp, core, asset, editor]
---

# AuthoredNodeMetadata

> **Header:** `Code/Include/GOAT/Assets/ProgramAsset.h`
> **Kind:** Plain struct, public API

---

## Overview

Editor-only data that a graph tool round-trips. **The runtime never reads it.**

```cpp
struct AuthoredNodeMetadata final
{
    //! Where the node sits on a graph canvas.
    AZ::Vector2 m_position = AZ::Vector2::CreateZero();
    //! Author's note about the node.
    AZStd::string m_comment;
};
```

It exists now, ahead of the graph tool, so that a saved `.goat` file does not change format the day
one arrives. Compiling ignores it entirely.

---

## Related

- [[AuthoredNode]]
- [[ProgramAsset]]

---

*Last updated: 2026-08-27*
