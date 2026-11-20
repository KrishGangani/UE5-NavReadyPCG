# 01b · Research Findings (Phase 2): NavReadyPCG

**Phase:** 2 (Research), completed 2026-11-21 · **Engine:** UE 5.8.4 (CL 58902052), read from `C:\Program Files\Epic Games\UE_5.8\Engine`

**Method:**
- Four investigations: PCG runtime generation, navigation rebuilds, existing work (engine, docs, forum, community), and measurement.
- Every engine claim cites a path relative to `Engine/` plus a line number in 5.8.4.
- The claims that drive design decisions were **re-checked by hand** before being written here; they are marked ✔.
- Claims drawn from following code flow rather than a direct statement are marked **[inferred]**. Claims not checked at all are **[unverified]**. Hypotheses that need a measurement are **[to measure]**.

**Why this phase matters.** The plan in the README was written from search snippets and a first source pass. Phase 2 found that one of its three engine claims is wrong as stated, that the "forum workaround" is a one-line suggestion whose default form rebuilds the whole navmesh, and that a global lock is the wrong tool for per-cell readiness. Nine findings changed the design; four changed the benchmark plan.

---

## 1. What Phase 2 changed

| # | Finding | Consequence for NavReadyPCG |
|---|---|---|
| 1 | **The PCG module has no navigation dependency and never calls a build lock** ✔ (`Plugins/PCG/Source/PCG/PCG.Build.cs:13–38`; grep of every PCG-family plugin) | The README's first claim holds. PCG output reaches navigation only through ordinary component registration, so coordination must come from outside PCG |
| 2 | **"Procedural instances never affect navigation" is wrong as a general statement** ✔. Only the GPU/procedural path is hard-wired off (`PCGProceduralISMComponent.cpp:135–136`, `.h:117`). The CPU Static Mesh Spawner creates ordinary (H)ISM components: PCG's default descriptor uses `NoCollision` (`PCGISMDescriptor.cpp:11–19`), so they are not navigation-relevant by default **[inferred]**, but with Pawn-blocking collision they are, and every `AddInstances` call (default `bUpdateNavigation = true`) notifies navigation (`PCGStaticMeshSpawner.cpp:887`; `InstancedStaticMeshComponent.h:275`; `InstancedStaticMesh.cpp:3731–3803`; `PrimitiveComponent.cpp:3672–3687`) | README corrected. The workload uses the **CPU** spawner with collision that blocks Pawn, so generated props really change the navmesh. GPU spawning is out of scope: Epic's docs say GPU instances support neither collision nor navigation |
| 3 | **Releasing a build lock with the default argument rebuilds the whole navmesh** ✔. `RemoveNavigationBuildLock(Flags, RebuildAction = Rebuild)` calls `RebuildAll()` (`NavigationSystem.cpp:4814–4836`), which releases the Detour navmesh and marks all bounds dirty (`RecastNavMeshGenerator.cpp:5797, 5802–5803`; `NavigationData.cpp:605–637`). Epic's documented `ReleaseInitialBuildingLock()` does the same (`NavigationSystem.cpp:4094–4097`) | The pipeline never uses the default release. Baseline **B1** implements the workaround with default arguments, as a user following the forum or docs would; **B2** uses `NoRebuild`. Hypothesis H3 is now code-backed: during a full rebuild the navmesh is **empty everywhere**, not only near fresh content **[inferred]** |
| 4 | **A build lock is global** ✔: it pauses dirty-area processing and tile generation for the whole world (`NavigationSystem.cpp:1676–1794`), while octree updates continue (`:1662–1673`) | A lock can't give *per-cell* control: one cell still generating blocks every finished cell. **The pipeline replaces "lock the navmesh" with a per-cell navigation commit** (finding 5). Locks stay as baselines B1/B2 |
| 5 | **A component's navigation relevance can be switched on later**: `SetCanEverAffectNavigation(true)` on a registered component calls `FNavigationSystem::OnComponentRegistered` (`ActorComponent.cpp:3266–3296`), and PCG's descriptor exposes `bCanEverAffectNavigation` per mesh entry (`ISMComponentDescriptor.h:236–237`, applied at `ISMComponentDescriptor.cpp:446–449`) **[inferred that the round trip works; tested in S5]** | **The core mechanism.** Generated props are spawned navigation-invisible (collision already on). When a cell finishes generating, NavReady "commits" it: it flips its PCG components to navigation-relevant, at most `nr.CommitsPerFrame` cells per frame. Each cell enters navigation once, complete, and other cells are never blocked |
| 6 | **The engine's readiness signals are global** (`IsNavigationBuildInProgress`, `OnNavigationGenerationFinishedDelegate`: `NavigationSystem.cpp:4889–4911`, `NavigationSystem.h:443–444`), **but a per-area query exists**: `FRecastNavMeshGenerator::HasDirtyTiles(const FBox&)` checks pending and running tiles in a box (`RecastNavMeshGenerator.h:830`, `.cpp:6644–6669`) ✔ | Per-cell readiness = at least one navigation tick since commit **and** no dirty areas queued **and** building not locked **and** `HasDirtyTiles(CellBounds) == false`. Baseline **B3** gates AI on the global signal instead, to measure what per-cell tracking buys |
| 7 | **Navigation ticks before the actor tick groups** (`LevelTick.cpp:1641–1647`); the PCG subsystem is a tickable object that runs after `TG_PostPhysics` (`LevelTick.cpp:1821`; `PCGSubsystem.h:77`). **Octree registration is deferred** to the next navigation tick (`NavigationDataHandler.cpp:68–118, 581–598`), with no public way to see pending updates (`NavigationSystem.h:1237, 1258`) | The readiness predicate requires **≥ 1 navigation tick after commit**; otherwise a cell could look "clean" before navigation has even seen it. NavReady's own subsystem is also a tickable, so it runs in the same phase as PCG |
| 8 | **Runtime generation only ticks when an `APCGWorldActor` is registered** (`PCGSubsystem.cpp:425–428`), and at runtime nothing creates one (`:431–443`). The actor registers itself in `BeginPlay` (`PCGWorldActor.cpp:187–191`) | The project has no hand-authored map yet, so step **S1** is a spike: spawn the world actor, navigation bounds and floor at runtime in a packaged build. **Fallback** if that fails: a map generated by an editor Python commandlet (still reproducible, run through the queue) |
| 9 | **PCG's subsystem-level "generation done" delegates exist only in editor builds** (`PCGSubsystem.h:52–71`); per-component native delegates exist at runtime (`PCGComponent.h:55–58, 380–383`) | Each cell is **one non-partitioned `GenerateAtRuntime` component on a cell actor**, so the pipeline can bind that component's delegates. A single partitioned component (local components on pooled partition actors) is the production growth path (`02` §11) |
| 10 | **Tile rebuilds invalidate paths that cross them** (`RecastNavMesh.cpp:2986–2989`), **but a query issued before the rebuild still uses the stale tiles** **[inferred]** | Path-failure checks need a **geometry test** (does a returned path pass through a generated prop?), not only `Result`/`IsPartial()` |
| 11 | **`pcg.FrameTime` (5 ms) budgets only the graph executor** ✔ (`PCGGraphExecutor.cpp:81–84, 1719–1726`; `IPCGBaseSubsystem.cpp:83–104`). The runtime-generation scheduler runs *after* the executor in the same tick and applies the deadline **only to cleanup** (`PCGSubsystem.cpp:421–428`; `PCGRuntimeGenScheduler.cpp:405–418, 1158–1166`). New generations are throttled by count (`NumGeneratingComponents` = 16, `:67–70, 1208–1215`) | The README's third claim holds, stated precisely (§6). The benchmark reports PCG, navigation and NavReady costs separately, because no single engine budget covers the whole pipeline |
| 12 | **The forum "workaround" is a one-line suggestion without code**, and the "20 frames back" quote that the portfolio research attributed to it comes from a thread **that is not about PCG** (forum 2704680) | README and `01` reworded; the portfolio research file was corrected the same day. Gap strength revised to **Medium** (§5) |

**Benchmark-plan changes:**

| # | Finding | Consequence |
|---|---|---|
| B-1 | Navigation tiles are built by **async worker tasks** (`RecastNavMesh.h:28–40`: `RECAST_ASYNC_REBUILDING 1`, time-sliced regeneration off) and PCG's budget uses `FPlatformTime::Seconds()`; both run in **real time**, while `-deterministic` fixes **game time** per frame (`UnrealEngine.cpp:3015–3023`) | Time-to-navigable is reported in **wall-clock ms** (plus frames). Game-time durations would reward a slow machine |
| B-2 | Costs land in separate CSV columns: `Exclusive/GameThread/NavigationBuild` (`LevelTick.cpp:1645`), `Exclusive/GameThread/Tickables` (includes the PCG subsystem; `Tickable.cpp:173`), `NavigationBuildDetailed/*` and `NavTasks/*` (on by default; `NavigationSystem.cpp:176–178`). PCG itself has **no** CSV stats | The per-subsystem split comes from the CSV. A finer PCG split (executor vs scheduler) is an Insights spot check only |
| B-3 | Hybrid CPU: UE5-UpdateBudget saw runs 1.6× apart when the game thread landed on an E-core | The batch script pins itself once to the P-cores, mask **`0xFFFF`** (`Tools\cpu_affinity.py`, self-tested 2026-11-21); every game it starts with `subprocess.Popen` inherits the mask from its first instruction, with a timeout and a kill in `finally`. Not `start /wait /affinity`, which hides the exit code |
| B-4 | `HasDirtyAreasQueued` is global and the dirty-area flush runs at most 60 times per second (`NavigationDirtyAreasController.h:25`) | The readiness probe can be at most about one frame late because of another cell's areas. Accepted and stated (`02` §12) |

---

## 2. PCG runtime generation (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| `pcg.FrameTime` 5.0 ms; `pcg.EditorFrameTime` 15.0 ms (Epic's docs page shows 16.667 / 50 ms: the source wins) | `Plugins/PCG/Source/PCG/Private/Graph/PCGGraphExecutor.cpp:81–84, 122–125` ✔ |
| Budget = `now + pcg.FrameTime`; the executor runs at least one loop, then while `now < EndTime` | `IPCGBaseSubsystem.cpp:83–104`; `PCGGraphExecutor.cpp:2209–2215, 2238–2252` ✔ |
| Order in `UPCGSubsystem::Tick`: refresh dirty runtime sources → base tick → **executor (budgeted)** → tracking → **runtime-gen scheduler** | `PCGSubsystem.cpp:364–429` ✔ |
| Scheduler: only `TickCleanup` checks the deadline; the comment says "Only apply time budget to cleanup currently" | `PCGRuntimeGenScheduler.cpp:405–418, 1158–1166` ✔ |
| The Static Mesh Spawner's prepare and execute phases are main-thread only | `PCGStaticMeshSpawner.cpp:721–727` |
| Worker tasks have their own 5 ms slice (`pcg.Async.OutOfTickBudgetInMilliseconds`) | `PCGAsync.cpp:25–29` |
| Generation trigger `GenerateAtRuntime`; default seed 42; per-component native delegates | `PCGComponent.h:77–82, 314, 55–58, 380–383` ✔ |
| Graphs can be built in code at runtime (`AddNodeOfType`, `AddEdge` outside `WITH_EDITOR`) | `PCGGraph.h:454, 477` |
| Generated components are tagged `"PCG Generated Component"` | `PCGHelpers.h:35`; `PCGActorHelpers.cpp:117` |
| Position seeds truncate to `int` per axis | `PCGHelpers.cpp:50–53` |

## 3. Navigation (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| Lock flags include `Custom = 1 << 4` for game use | `Source/Runtime/NavigationSystem/Public/NavigationSystem.h:102–112` ✔ |
| `ELockRemovalRebuildAction { Rebuild, RebuildIfNotInEditor, NoRebuild }`; default `Rebuild` | `NavigationSystem.h:1146–1153` ✔ |
| Default release → `RebuildAll()` → Detour navmesh released, all bounds dirty | `NavigationSystem.cpp:4827–4833`; `RecastNavMeshGenerator.cpp:5797–5803` ✔ |
| While locked: octree updates still run; dirty-area flush and tile builds pause | `NavigationSystem.cpp:1662–1673, 1676–1794` ✔ |
| Dirty areas flushed at most 60 times per second | `NavigationDirtyAreasController.h:25`; `.cpp:53–110` |
| Engine default runtime generation is `Static`; PCG content needs `Dynamic` | `BaseEngine.ini:3042–3043`; `NavigationData.h:523–533`; forum 2693069 |
| Tiles: async workers, cap `min(2 × workers, 1024)`, geometry gathered on the game thread by default | `RecastNavMesh.h:28–40`; `RecastNavMeshGenerator.cpp:5465, 5891–5897`; `RecastNavMesh.cpp:549–550` |
| Per-area dirty-tile query `HasDirtyTiles(FBox)` and count `GetDirtyTilesCount(FBox)` (public) | `RecastNavMeshGenerator.h:829–831` ✔ |
| Path invalidation on rebuilt tiles | `RecastNavMesh.cpp:2986–2989` |
| `SetRebuildingSuspended` is another global pause, capped at 32,768 cached areas, past which it rebuilds everything | `NavigationData.h:717–729`; `NavigationData.cpp:36–42` |
| `FindPathSync`, `FPathFindingResult::IsPartial`, `ENavigationQueryResult` | `NavigationSystem.h:650, 658`; `NavigationData.h:57–70`; `NavigationTypes.h:638–647` |

## 4. Measurement (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| Fixed step: `-deterministic` = fixed time step + fixed seed; delta set to 1/FPS, no waiting | `LaunchEngineLoop.cpp:2455–2462`; `UnrealEngine.cpp:3015–3023` ✔ (re-opened; first found by UE5-UpdateBudget) |
| Event-driven CSV capture: `-csvStartOnEvent=`, `-csvCaptureOnEventFrameCount=`, `-ExitAfterCsvProfiling` | `CsvProfiler.cpp:5078, 5081, 4309` ✔ |
| `Exclusive` category on by default; `NavigationBuild` and `Tickables` exclusive stats | `CsvProfiler.cpp:73`; `LevelTick.cpp:1645`; `Tickable.cpp:173` ✔ |
| Navigation detail stats, tile task counts | `NavigationSystem.cpp:175–178, 1664–1692, 1796–1811` |
| Packaged Development, because the launcher engine can't build Test | UE5-UpdateBudget `09` F-3a; [research/04](Research/04_local_engine_verification.md) §0b |

## 5. Existing work, updated

| Solution | Per-cell control? | Readiness signal | AI gating | Measured? | Evidence |
|---|---|---|---|---|---|
| PCG runtime generation scheduler | n/a (no navigation) | none | none | Epic: up to 30% less scheduler game-thread time in 5.8 | source; 5.8 release notes |
| Dynamic navmesh, uncoordinated (engine default once `Dynamic` is chosen) | no: rebuilds while the cell is still being generated | global only | none | — | source |
| Build lock + default release (docs: `bInitialBuildingLocked` + `ReleaseInitialBuildingLock`; forum suggestion) | no: global | global | none | no published numbers for PCG | docs (labelled 5.9); forum 2693069; source |
| Build lock + `NoRebuild` | no: global | global | none | — | source |
| `SetRebuildingSuspended` | no: per navigation data | global | none | — | source |
| Dynamic Modifiers Only + modifiers on PCG objects | n/a | — | none | — | forum 2693069 (needs a pre-built base navmesh) |
| GPU spawning, FastGeo (Beta / Experimental) | — | — | — | — | **no navigation at all** (docs, source) |
| "Runtime Navigation Area Modifiers" (Gumroad) | area modifiers only | — | — | — | snippet only |

**The gap, stated conservatively:**
- The engine provides every building block: generation, rebuilds, a global pause, global "done" signals and a per-area dirty-tile query.
- Among the public sources found, none connects them into a **per-cell handshake** (content complete → handed to navigation → tiles rebuilt → AI may use it), and none **measures** what the uncoordinated default costs in path failures, time-to-navigable and worst frame.
- A web search can't prove absence, and studio-internal systems aren't public. This is a statement about what was found.
- **Gap strength: Medium** (the roadmap said Medium–Strong). The building blocks are public engine APIs. The value is in assembling them correctly (the default release rebuilds everything; the global signals mislead under load) and in measuring honestly.

## 6. The three README claims, checked

| README claim | Verdict | Precise statement (5.8.4) |
|---|---|---|
| "PCG uses no nav build locks" | **Correct** ✔ | The PCG plugin family never references build locks, and the PCG module doesn't depend on `NavigationSystem` at all (finding 1). PCG output reaches navigation only through generic component registration |
| "Procedural instances never affect navigation" | **Wrong as stated** ✔ | True only for GPU/procedural ISM output (hard-coded off) and, by the same path, componentless FastGeo output. CPU Static Mesh Spawner output is ordinary (H)ISM: not navigation-relevant with PCG's default `NoCollision` descriptor, navigation-relevant (and updated on every `AddInstances`) when its collision blocks Pawn. Spawned actors affect navigation like any actor (finding 2) |
| "The PCG budget ignores scheduler time" | **Correct, with one nuance** ✔ | `pcg.FrameTime` budgets the graph executor. The scheduler runs after it in the same tick; only its cleanup step checks the deadline. Queueing and scheduling new generations are throttled by count (16), not time. Navigation work caused by the output happens in the navigation tick, outside PCG's accounting (finding 11). Epic's Julien_LHeureux said the same on the forum ("I don't think the budget accounts for the time spent in the runtime gen scheduler") |

## 7. PCG feature maturity in 5.8 (what this project uses, and what it avoids)

| Feature | Label in 5.8.4 | Used here? | Evidence |
|---|---|---|---|
| PCG framework (plugin `PCG`), CPU graph execution | **Production-Ready** (since 5.7); no plugin flag; enabled by default | yes | `PCG.uplugin:13–15`; Epic 5.7 announcement |
| Runtime generation scheduler, Create Points Grid, Transform Points, CPU Static Mesh Spawner | no Beta/Experimental label found in the code or docs read | yes | source headers (no marker) |
| GPU processing | **Beta** | no | Epic GPU docs |
| GPU Static Mesh Spawner | **Experimental** | no | Epic GPU docs |
| Actor/componentless runtime generation | **Experimental** | no | `PCGComponent.h:849`; `PCGGraph.h:805` |
| FastGeo, Instanced Actors, Nanite Assemblies, Niagara, Water interops; Biome, Primitives, Mesh Partition, Toolset | **Experimental** (plugin flags) | no | their `.uplugin` files |
| Geometry Script, External Data, Python interops | **Beta** (plugin flags) | no | their `.uplugin` files |

Details and line numbers: [Phase2_Raw_Notes/A_pcg_runtime.md](Phase2_Raw_Notes/A_pcg_runtime.md) §A8. **APIs are pinned to 5.8.4**: PCG deprecated several cvars in 5.8 (`pcg.MaxConcurrentTasks` → `pcg.Editor/Runtime.MaxProcessingTasks`, `PCGGraphExecutor.cpp:138–155`), so nothing here is assumed to carry over to 5.9.

## 8. Hypothesis status after Phase 2

| # | Hypothesis | Status |
|---|---|---|
| H1 | Uncoordinated output: navigation starts rebuilding while a cell is still being generated, and agents query stale tiles, so paths fail near fresh content | **Supported by code** (no coordination; every `AddInstances` notifies navigation) **[to measure]** |
| H2 | Incremental output re-dirties the same tiles more than once per cell | **Plausible from code** (one notification per ISM component per `AddInstances`; flush up to 60 Hz) **[to measure]** |
| H3 | The default lock release rebuilds the whole navmesh, which is empty while it rebuilds | **Supported by code** (finding 3) **[to measure: magnitude, and failures far from fresh content]** |
| H4 | Global readiness signals delay AI under continuous generation | **Supported by code** (signals are global) **[to measure]** |
| H5 | Runtime generation costs more game-thread time per frame than `pcg.FrameTime` suggests | **Verified for the scheduler by code**; navigation adds more **[to measure: magnitude]** |
| H6 | Same seed gives the same content on one machine; instance order inside a component may differ | Unchanged **[to measure]** |

## 9. Carried into Phase 3

- Cells: one `GenerateAtRuntime` PCG component per cell actor; graph built in code (Create Points Grid → Transform Points → Static Mesh Spawner, collision `BlockAll`, navigation flag per configuration).
- Pipeline: commit per cell (`SetCanEverAffectNavigation(true)`), at most `nr.CommitsPerFrame` per frame; per-cell readiness predicate (finding 6 + the ≥ 1 navigation tick rule); content hash; AI-ready event.
- Baselines: B0 uncoordinated, B1 lock + default release, B2 lock + `NoRebuild`, B3 global readiness gate.
- S1 spike for the runtime-spawned world (finding 8), with the editor-commandlet map as the fallback.
- Measurement: wall-clock time-to-navigable, CSV per-subsystem split, P-core pinning, path checks with a geometry test.

**Raw research notes**, with every quote, are in [Phase2_Raw_Notes/](Phase2_Raw_Notes/README.md) as an audit trail. This file is authoritative: everything the design depends on is restated above with its evidence.
