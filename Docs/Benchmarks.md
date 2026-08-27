# Baseline

Numbers to measure the optimisation work against. Taken before any of it started.

```
GOATSystemComponent has no part in these: the benchmark builds DecisionProgram by hand and
leaves LuaDispatch unconnected, so what is measured is the gem's decision path rather than
O3DE's asset, entity and scripting stacks.
```

Run them with:

```
cd <build>/bin/profile
./AzTestRunner libGOAT.Tests.so AzRunBenchmarks
```

**Profile build only.** A debug build measures the ~400 asserts instead of the code.

## 2026-08-26 — before Stage 1

Ryzen, Linux, profile, single threaded.

| Benchmark | 100 | 1,000 | 10,000 |
|---|---|---|---|
| `BM_TickRunning` (ns, whole population) | 15,796 | 161,250 | 1,673,168 |
| — per agent-tick | 158 ns | 161 ns | 167 ns |
| `BM_SpawnAgents` bytes/agent | 656 | 654 | 653 |
| `BM_AgentWrite` per write | 5.2 ns | 5.7 ns | 8.4 ns |

| `BM_TreeWalk` width | 4 | 30 | 100 |
|---|---|---|---|
| ns per walk | 82.5 | 987 | 8,800 |

## What the baseline already says

**The tree walk is superlinear in a composite's width.** 4 → 30 is 7.5× the branches for 12× the
time; 30 → 100 is 3.3× for 8.9×. A walk that bubbles a result out of a child finds out which child
it was by walking the sibling chain (`TreeWalker::ChildIndexOf`), so a wide selector costs
O(children²). This is the number Stage 5's per-node `m_childOrdinal` exists to move, and it was a
prediction from reading the code before it was a measurement.

**Per-agent cost degrades with population**: 158 → 167 ns per agent-tick from 100 to 10,000, on
identical work. That is the cache falling out from under scattered per-agent allocations, and it is
what stages 3 to 6 address.

`bytes/agent` counts only what this fixture allocates — an agent's blackboard storage and its cursor.
It does **not** include `AgentRecord` (~1200 B) or the registry, because the benchmark drives the
decision path directly. An allocation count belongs beside it and is deliberately absent until it can
be taken from the allocator rather than guessed.
