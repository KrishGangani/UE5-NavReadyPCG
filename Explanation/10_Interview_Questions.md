# 10 · Interview Questions: NavReadyPCG

Answers grounded in this project's code (UE 5.8.4) and in what was measured ([09](09_Testing_And_Benchmarks.md)). Where something was not measured, the answer says so.

**1. Why doesn't "lock navigation, generate, unlock" solve runtime PCG + navigation?**
A navigation build lock is global: one region still generating holds back every finished one. And `RemoveNavigationBuildLock` with its default action calls `RebuildAll()`, the whole navmesh (`NavigationSystem.cpp:4814–4836`). NavReady works per cell: props are spawned navigation-invisible and handed to navigation together when the cell is complete.

**2. How do you know a region's navmesh is current?**
Not from a timer, and not from the global "build in progress" signal (one busy region hides every finished one). Each frame the probe checks: navigation has ticked since the commit, no lock is held, no dirty areas are queued, and `FRecastNavMeshGenerator::HasDirtyTiles(Bounds)` reports nothing inside the cell's bounds grown by the navmesh border (`05` D3).

**3. Why grow the bounds?**
The engine grows every dirty area by the tile border before marking tiles, so a prop near a cell edge dirties the neighbour's tile. It grows by (high, low), while the exported `GrowBoundingBox` grows by (low, high), so the code takes the larger growth on both sides.

**4. Why is a cell never ready in its commit frame?**
Navigation ticks before the tick groups and registers new content in that tick, so a commit in frame N is first seen in N + 1. Unit test `NavReadyPCG.Cell.NotReadyInCommitFrame`.

**5. How do you prove the content is deterministic?**
A per-cell hash over quantized world transforms (1 mm, 1e-4 per quaternion component, 1e-3 scale), with the quaternion sign made canonical and the mesh path, not a pointer, hashed; instance hashes are sorted, so order doesn't matter. Measured: identical in 16/16 cells across runs, with and without the GPU, cooked and uncooked; another seed changes all 16 (`09` S2).

**6. What surprised you in the engine?**
`TileSizeUU` 1000 gives 988 cm tiles; the Volume Sampler's lattice is world-aligned; centre-pivot shapes at scale 0.6 sit under the 35 cm step height; ±50 cm of jitter enclosed a pocket; without the GPU, game time runs about 2.5× faster than the wall clock (`MinUndilatedFrameTime`). Each was found by a check (`09` S2).

**7. Why real time at 60 fps instead of a fixed step for benchmarks?**
A fixed step never waits (`UnrealEngine.cpp:3005–3017`), so game time runs ahead of the wall clock by a factor that depends on each configuration's frame cost, while tile builds run in real time. The configurations would no longer face the same workload.

**8. What does NavReady cost per frame?**
Its own exclusive CSV stat, `Exclusive/GameThread/NavReady`; the measured value is in the README and `09`.

**9. What did you not measure?**
AI path correctness (M4) and activation latency (M2) need the measuring agents, and the B1–B3 baselines; both were cut when the project was closed early. `09` lists them as "not measured", not estimated.

**10. A bug your checks caught before it cost a measurement?**
Batch scripts: `call … || exit /b` returned 0 after a failed build under `cmd /c`; `exit /b %ERRORLEVEL%` inside a `( )` block returned the code from before the command; `%2 … %9` drop arguments past eight; a comma splits an argument. Each let a failing step look like a pass (`07`).
