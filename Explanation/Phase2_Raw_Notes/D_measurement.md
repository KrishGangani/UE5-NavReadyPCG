> **Raw Phase 2 research notes (D_measurement), UE 5.8.4, 2026-11-21.** Most measurement facts were first established by UE5-UpdateBudget (P01) on the same engine and machine. The ones this project relies on were **re-opened here** at the lines below. **The findings file [../01b_Research_Findings.md](../01b_Research_Findings.md) is authoritative.**

# P08 NavReadyPCG: Phase 2 research, part D: measurement

## D1. Inherited from UE5-UpdateBudget, re-opened on 5.8.4

| Fact | Source (re-opened 2026-11-21) |
|---|---|
| `-deterministic` = `-UseFixedTimeStep` + `-FixedSeed` | `Runtime/Launch/Private/LaunchEngineLoop.cpp:2455–2462` |
| In fixed-step mode the frame delta is set to the fixed value and the engine never waits ("it lie[s] about how fast it is going") | `Runtime/Engine/Private/UnrealEngine.cpp:3015–3023` |
| `-csvStartOnEvent=`, `-csvCaptureOnEventFrameCount=`, `-ExitAfterCsvProfiling` | `Runtime/Core/Private/ProfilingDebugging/CsvProfiler.cpp:5078, 5081, 4309` |
| The `Exclusive` CSV category is on by default | `CsvProfiler.cpp:73` |
| The launcher engine can't build the Test configuration, so headline builds are packaged **Development** | P01 `09` F-3a (build log); [research/04](../Research/04_local_engine_verification.md) §0b |

## D2. New for this project: where the costs show up in a CSV capture

| What | CSV column | Source |
|---|---|---|
| The whole navigation tick (octree updates, dirty areas, tile submission and collection) | `Exclusive/GameThread/NavigationBuild` | `Runtime/Engine/Private/LevelTick.cpp:1645` |
| All tickable objects, **including the PCG subsystem** (executor + runtime-gen scheduler) | `Exclusive/GameThread/Tickables` | `Runtime/Engine/Private/Tickable.cpp:173` |
| Navigation sub-steps | `NavigationBuildDetailed/Navigation_ProcessPendingOctreeUpdates`, `…_RebuildDirtyAreas`, `…_TickAsyncBuild` (category on by default) | `NavigationSystem.cpp:176, 1664, 1685, 1692` |
| Remaining tile tasks per navigation data | `NavTasks/NumRemainingTasks_<name>` (on by default, not in Shipping) | `NavigationSystem.cpp:178, 1796–1806` |
| Running tile tasks | `NavigationSystem/NumRunningTasks` (category off by default: enable with `-csvCategories=NavigationSystem`) | `NavigationSystem.cpp:175, 1810` |
| PCG has **no** CSV stats of its own; it has Insights scopes (`UPCGSubsystem::Tick`, `FPCGGraphExecutor::Execute`, `FPCGRuntimeGenScheduler::Tick`, …) | `TRACE_CPUPROFILER_EVENT_SCOPE` | `PCGSubsystem.cpp:366`; `PCGGraphExecutor.cpp:2189`; `PCGRuntimeGenScheduler.cpp:343`; a grep for `CSV_` in the PCG module is empty |

**Consequences for the plan:**
- The per-subsystem split comes for free: navigation from `NavigationBuild`, PCG (plus any other tickables) from `Tickables`. Our own pipeline work gets its own exclusive stat, which is subtracted from `Tickables` automatically (exclusive stats nest). **[inferred]** from how exclusive stats work; checked on the first capture.
- A finer PCG split (executor vs scheduler) needs an Unreal Insights trace. That is a spot check, not a per-run statistic.

## D3. Wall-clock time vs fixed-step time

- With `-deterministic -FPS=60`, every frame advances game time by exactly 1/60 s, however long the frame really took (D1).
- **Navigation tiles are built by worker tasks in real time** (notes B6), and PCG's budget is measured with `FPlatformTime::Seconds()` (notes A3). Both depend on wall-clock time, not game time.
- So "time-to-navigable" is reported in **wall-clock milliseconds** (`FPlatformTime::Seconds()` at each pipeline event), with the frame count alongside. Game-time values would be meaningless here: a fast machine would show "more game time" for the same real work. **[inferred]**

## D4. Hybrid CPU: pin timed runs to the performance cores

- The i7-14650HX has 8 P-cores (logical CPUs 0–15) and 8 E-cores (16–23). UE5-UpdateBudget's S3 batch showed runs at two stable levels about 1.6× apart when Windows placed the game thread on an E-core.
- The batch script pins **itself** once with `SetProcessAffinityMask(GetCurrentProcess(), mask)` before it records the environment or starts anything. A child process inherits its parent's affinity, so every game it then starts runs pinned from its first instruction, engine start-up included.
- It starts the **real** game executable (`Build\Windows\NavReadyPCG\Binaries\Win64\NavReadyPCG.exe`, not the bootstrap) with `subprocess.Popen`, waits with a timeout, and kills the tree in a `finally` block. The mask read back with `GetProcessAffinityMask` goes into the environment record.
- `cmd /c start "" /wait /affinity …` is **not** used: it returns exit code 0 whatever the program returned (found by UE5-TestTrust).
- Self-test on 2026-11-21: the script's mask went from `0xFFFFFF` (all 24 logical CPUs) to `0xFFFF` after `pin_self`; a PowerShell child reported `ProcessorAffinity` 65535 (= `0xFFFF`), so the child inherited it; `run_game` returned exit code 3 for `cmd /c exit 3`, and `None` (process killed) on a 1 s timeout.
- The mask comes from `GetLogicalProcessorInformationEx` (`Tools\cpu_affinity.py`, copied from UE5-UpdateBudget). On 2026-11-21 it printed **`0xFFFF`**.
- Builds and the Phase 4 smoke runs are not timed, so they are not pinned.

## D5. What a path "failure" can be checked against

- `FPathFindingResult::Result` and `IsPartial()` (notes B8) catch "no path" and "partial path".
- Neither catches a **stale** path: a complete, successful path through an obstacle that the navmesh doesn't know about yet. That needs a geometry check against the generated props (a line trace along each path segment). Planned in `02` §4; the exact trace setup is verified in step S4.
