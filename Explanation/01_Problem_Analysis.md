# 01 · Problem Analysis: NavReadyPCG

**Phase:** 1 (Problem Analysis), 2026-11-21; updated after Phase 2 the same day (evidence in [01b_Research_Findings.md](01b_Research_Findings.md))
**Engine:** UE **5.8.4** (CL 58902052). Engine facts are read from the local source and cited as `File:line` (paths relative to `Engine/`).
**Status of every claim:** marked **[verified]** (read in engine source or a cited source), **[hypothesis]** (to be confirmed by measurement), **[inferred]** (reasoned from code, not read directly), or **[target]** (a pass/fail threshold, not a result).

---

## 1. The problem in one paragraph

Runtime procedural generation (PCG) fills a region of the world with props while the game is running: rocks, crates, ruins, debris. If those props block movement, the navigation mesh must be rebuilt around them before AI can path through the region correctly. In UE 5.8 nothing connects the two systems. PCG never talks to navigation [verified]; the navigation system simply rebuilds whatever tiles become dirty, while the region is still half-generated, and it has only **global** signals for "done" [verified]. So an AI agent that starts moving as soon as the content appears either gets a path straight through a rock it can't see yet, or gets no path at all; and a game that waits for "navigation finished" may wait for unrelated areas too. The common advice, "lock navigation while PCG runs, then rebuild", has a trap: releasing the lock with its default argument throws away and rebuilds the **whole** navmesh [verified]. Meanwhile PCG's own frame budget covers only part of PCG's game-thread work [verified], so nobody sees the full cost of a region becoming playable.

## 2. Where this happens in real games

| Evidence | What it shows | Source |
|---|---|---|
| CD PROJEKT RED, Expert Gameplay Programmer (AI) | Lists "spawning and systemic open world" and "procedural content generation" together with AI | [research/01](Research/01_job_market.md) C4 |
| CD PROJEKT RED, Senior Gameplay Programmer (AI) | "core navigation, pathfinding, and avoidance systems, with a focus on scalability and open world performance" | [research/01](Research/01_job_market.md) C3 |
| Rocksteady, Senior Gameplay Programmer | "coordinating combat-related and navigation related NPC behaviours" | [research/01](Research/01_job_market.md) WB1 |
| Rockstar North, Senior Gameplay Systems Programmer | "open-world, population and streaming systems" | [research/01](Research/01_job_market.md) RS2 |
| Epic forum, "DynamicModifierOnly and Runtime navmesh building (PCG)" (Dec 2025) | A developer generating a level with PCG couldn't get navmesh after generation; the advice was Dynamic navmesh plus "lock the navmesh while PCG runs, then release the lock and rebuild the navmesh in the dirty area afterward", with no code | [01b](01b_Research_Findings.md) §1 #12; [notes C2](Phase2_Raw_Notes/C_existing_work.md) |
| Epic forum, "Runtime Cost in PCG Hierarchical Generation" (2025–26) | A team budgeting 500 µs for PCG on PS5 still saw ~1 ms spikes; Epic: "I don't think the budget accounts for the time spent in the runtime gen scheduler" | [research/02](Research/02_technical_challenges.md) [T10-B]; [notes C2](Phase2_Raw_Notes/C_existing_work.md) |

**Honest note on hiring evidence:** in the 57 postings sampled, game AI and navigation appear in 20 (35%), but procedural generation is named in only 1 and the PCG framework in none ([research/01](Research/01_job_market.md) §4, §7). The transferable skill this project shows is **coordinating two engine systems that don't know about each other, under a frame budget, and proving it with measurements**. PCG and navigation are the vehicle.

## 3. What already exists, and why it doesn't fully solve the problem

| Existing tool | What it does | What it doesn't do | Evidence |
|---|---|---|---|
| PCG runtime generation scheduler | Generates components around generation sources (by default, player controllers), at most 16 at a time | Knows nothing about navigation | [verified] `01b` §2 |
| `pcg.FrameTime` (5 ms) | Time-slices the PCG graph executor | Doesn't cover the scheduler (only its cleanup checks the deadline) or the navigation work caused by the output | [verified] `01b` #11 |
| Dynamic navmesh (`RuntimeGeneration=Dynamic`) | Rebuilds dirty tiles on worker threads | Rebuilds as soon as anything changes, including half-generated content. The engine default is `Static`, which never rebuilds | [verified] `01b` §3 |
| `AddNavigationBuildLock` / `RemoveNavigationBuildLock` | Pause and resume rebuilding | Global: one generating region blocks every other region. **The default release calls `RebuildAll()`**, which discards the whole navmesh | [verified] `01b` #3, #4 |
| `bInitialBuildingLocked` + `ReleaseInitialBuildingLock()` (Epic's documented pattern) | The same lock with the `InitialLock` flag | Same full rebuild on release in 5.8.4. The docs' tip to reset the dirty areas first needs a subclass (the member is protected) | [verified] `01b` §5 |
| `SetRebuildingSuspended` on a navigation data | Caches dirty areas while suspended | Global for that navmesh; doesn't stop builds in progress; past 32,768 areas it rebuilds everything | [verified] `01b` §3 |
| `IsNavigationBuildInProgress()`, `OnNavigationGenerationFinishedDelegate` | Tell you navigation is busy or done | Global: under continuous generation, "done" may never come while any other area is building | [verified] `01b` #6 |
| Dynamic Modifiers Only + navigation modifiers on PCG objects | Cheap obstacle and area changes | Needs a base navmesh built in advance; can't add navmesh for new geometry | forum 2693069 |
| GPU spawning, FastGeo componentless output (Beta / Experimental) | Fast scatter | **No collision and no navigation at all** | [verified] `01b` #2 |
| "Runtime Navigation Area Modifiers" (Gumroad) | PCG-integrated navigation area modifiers | Not rebuild orchestration or readiness | snippet only, [research/03](Research/03_existing_solutions_gap_analysis.md) §11.4 |

**The gap (medium strength, after Phase 2):**
- Every building block is a public engine API, including a per-area "are tiles still building here?" query.
- Among the public sources found, nothing connects them into a **per-region handshake**: content complete → handed to navigation → tiles rebuilt → AI allowed in.
- Nothing published **measures** what the uncoordinated default costs.
- The roadmap rated this gap Medium–Strong. Phase 2 lowers it to **Medium**: an experienced engineer could assemble this. The value is in doing it correctly (avoiding the full-rebuild trap and the misleading global signals) and in measuring it honestly.

## 4. Root-cause hypotheses

Each hypothesis gets a measurement that can confirm or reject it. The baseline runs come before any pipeline code.

| # | Hypothesis | Why we suspect it | How we test it |
|---|---|---|---|
| H1 | **Uncoordinated output makes AI path through stale navigation.** Navigation starts rebuilding while a region is still being generated, and agents that start moving at once get paths through props that aren't in the navmesh yet | PCG never coordinates [verified]; every `AddInstances` call notifies navigation [verified]; a rebuild invalidates paths only *after* it finishes [verified] | B0: count path failures (§6.3) in the first seconds of agent activity near fresh content |
| H2 | **Incremental output rebuilds the same tiles more than once per region** | One notification per ISM component per `AddInstances` call; dirty areas are flushed up to 60 times per second [verified] | Tile rebuilds per cell (§6.3, M7) in B0 vs the pipeline |
| H3 | **The default lock release rebuilds the whole navmesh, and the navmesh is empty while it does** | `RemoveNavigationBuildLock(…, Rebuild)` → `RebuildAll()` → the Detour navmesh is released [verified]; emptiness while rebuilding is [inferred] | B1: time-to-navigable, worst frame, and path failures **far from** fresh content |
| H4 | **A global readiness gate delays AI for regions that are already ready** | The engine's signals are global [verified] | B3 vs the pipeline: time from generation complete to AI active, in the burst phase |
| H5 | **Making a region playable costs more game-thread time than `pcg.FrameTime` suggests** | The scheduler runs outside the executor's budget [verified]; navigation work is outside PCG entirely [verified] | Per-frame `Tickables` (PCG) + `NavigationBuild` (navigation) during generation vs the 5 ms budget |
| H6 | **The same seed gives the same content on one machine, but instance order inside a component may vary** | Seeds come from settings and positions [verified]; execution is multithreaded [verified] | Content hash (order-independent) and an order-dependent hash, across runs, configurations and builds |

**Not a cause we accept without checking:** "the GPU is the bottleneck". Every capture records GPU time. The problem here is correctness and game-thread spikes; if GPU time dominated frame time, worst-frame comparisons would be meaningless, and the scene would be changed.

## 5. The cost of leaving it unsolved

- **Visible AI bugs.** Agents walk into or through newly generated obstacles, or stand still because they got no path. Players read this as broken AI, not as a navigation timing issue.
- **Workarounds that hurt.** "Lock and rebuild" with default arguments rebuilds the whole navmesh: a hitch, and a window in which **every** agent in the world has no navmesh [inferred, H3]. "Wait until navigation is idle" can stall AI indefinitely while the player keeps streaming in new content [H4].
- **Hidden cost.** A team that budgets PCG with `pcg.FrameTime` is not budgeting the scheduler or the navigation rebuild its content causes [verified, H5].
- **No regression signal.** Without a determinism check, a content or engine change that alters generated output goes unnoticed until saved games, replays or networked clients disagree.

*Arithmetic illustration only, not a measurement:* if a 30 m × 30 m region covers 9 navmesh tiles and the engine's own fallback estimate of 2.5 ms per tile applies (`NavigationSystem.cpp:628`, "use default of 2.5 milli secs to regen a full tile", a default of the time-sliced regeneration manager, which is inactive in this build), one region costs about 22 ms of tile work; spread over worker threads that is fast, but a full `RebuildAll` of a 200 m × 200 m navmesh (400 tiles) is about 1 s of tile work, and the navmesh is incomplete for that whole time.

## 6. What "solved" means: success criteria, defined now

### 6.1 The workload (identical in every run)

- **Arena:** a flat 200 m × 200 m floor with collision, covered by navigation bounds. Navmesh `RuntimeGeneration = Dynamic`; every other navmesh setting at its engine default (10 m tiles, 19 cm cells, 34 cm agent radius; `BaseEngine.ini:3045–3066`).
- **Cells:** a 4 × 4 grid of 30 m × 30 m cells, centred: 16 cells, each aligned to a 3 × 3 block of navmesh tiles where the tile origin allows (checked in S2).
- **Content per cell:** one PCG graph, the same for every cell, seeded per cell:
  - a Volume Sampler filling the cell's box at about 3 m spacing (S1: Create Points Grid's settings class isn't exported) → Transform Points (seeded jitter, random yaw, scale 0.6–1.4) → a deterministic thinning to about half → Static Mesh Spawner;
  - two meshes (`/Engine/BasicShapes/Cube` and `Cylinder`), so two ISM components per cell;
  - collision `BlockAll`, so every prop changes the navmesh.
  - About 50 props per cell, about 800 in total. The exact density is **calibrated in S2** (like UE5-UpdateBudget's compute calibration): high enough that B0 shows the problem, low enough that cells don't enclose unreachable pockets.
- **Generation driver:** PCG's own runtime generation scheduler. The generation source is the player camera on a fixed path over the grid: a **cruise** phase (4 m/s, cells come into range one by one) then a **burst** phase (12 m/s, several cells generating at once). No cell is cleaned up during a run.
- **Agents:** 8 per cell (128 in total), spawned at seeded free positions when the cell's generation completes. When active, each agent asks for a path to its next seeded target in the cell every 0.5 s of game time and follows it at 3 m/s. Each agent is measured over its **first 6 s of activity**: 12 requests per agent, **1,536 measured requests per run**.

### 6.2 Compared configurations (all in the same build, switched by command line)

| ID | Navigation handling | When agents become active | Why it is included |
|---|---|---|---|
| **B0** | Dynamic navmesh, no coordination; props navigation-relevant from the start | at spawn | What 5.8 does out of the box |
| **B1** | B0 + `AddNavigationBuildLock(Custom)` while any cell generates; `RemoveNavigationBuildLock(Custom)` with the **default** action (`Rebuild`) when none do | at spawn | The forum/docs workaround, as written with default arguments |
| **B2** | Same lock, released with `ELockRemovalRebuildAction::NoRebuild` | at spawn | The corrected workaround: only dirty areas are rebuilt |
| **B3** | B0's navigation; agents wait for the engine's **global** signal (`!IsNavigationBuildInProgress()` and no dirty areas queued) | when navigation is globally idle | The engine's own readiness signal, used as a gate |
| **NR** | **NavReady**: props spawned navigation-invisible; each completed cell is committed to navigation (at most 1 cell per frame), tracked until its own tiles are rebuilt, hashed, then marked AI-ready | when **its own cell** is AI-ready | Our system |

The generated content is the same in all five (SC2 checks it). The only content difference is the navigation flag on the spawner's descriptor, which NR needs and which is excluded from the hash.

### 6.3 Metrics

| # | Metric | Unit | Tool |
|---|---|---|---|
| M1 | **Time-to-navigable** per cell: generation start → cell navigation-ready, split into generation (start → complete) and settle (complete → ready) | ms, wall clock (+ frames) | Pipeline events stamped with `FPlatformTime::Seconds()`, in the run report. The readiness probe is the same code in every configuration |
| M2 | **Time-to-AI-active** per cell: generation complete → agents active | ms, wall clock | Run report |
| M3 | **Worst frame during generation**: max, p99 and count of frames over 33.3 ms of `FrameTime` and `GameThreadTime`, over the generation window (first generation start → last cell ready) | ms, count | CSV profiler, `Tools\csv_stats.py` |
| M4 | **Path failures per 1,000 measured requests near fresh content**: F1 no path (`Result != Success`), F2 partial path, F3 a returned path passes through a generated prop (line trace along each segment) | per 1,000 | Agents' run report |
| M4b | Path failures **away from** fresh content (agents in already-ready cells), to test H3 | per 1,000 | Agents' run report |
| M5 | **Determinism**: per-cell content hash (order-independent) and order-dependent hash; match rate across runs, configurations and builds | % | Run report |
| M6 | **Cost split per frame**: `Exclusive/GameThread/Tickables` (PCG + other tickables), `Exclusive/GameThread/NavigationBuild`, and NavReady's own exclusive stat | ms | CSV profiler |
| M7 | **Navigation work**: tiles rebuilt per cell, remaining tile tasks per frame | count | `NavTasks/*` CSV stats; a tile counter if S6 adds one |

**What counts as a failure, exactly.** A request fails only if it is wrong **compared with the settled navmesh**. After a cell is navigation-ready (+0.5 s), every failed request's start and target are queried again. If the settled navmesh also fails (an enclosed target) or also routes through a prop (a navmesh-resolution artefact), the request is reclassified as "invalid target" and excluded from M4, and the exclusions are reported. This keeps M4 about *timing*, not about workload geometry.

### 6.4 Pass/fail criteria [target]

| # | Criterion | Threshold |
|---|---|---|
| SC1 | **AI never paths through unready content** | NR: **0** failures (F1–F3) per 1,000 measured requests in **every** run, with ≥ 1,000 measured requests per run. No tolerance |
| SC2 | **Determinism** | 100% of cells give the same content hash in every run of every configuration with the same seed, and in the editor and packaged builds; a different seed changes the hash of all 16 cells |
| SC3 | **Navigation readiness is not slower** | NR median time-to-navigable (M1, start → ready) ≤ 1.10 × B0's median, and NR p95 ≤ B0 p95. Against the default workaround: NR p95 ≤ 0.5 × B1 p95 |
| SC4 | **AI activates promptly** | NR median time-to-AI-active (M2) during the burst phase ≤ 0.5 × B3's median, with non-overlapping run ranges; NR p95 ≤ 300 ms |
| SC5 | **No worse worst frame** | During the generation window, NR max and p99 `GameThreadTime` ≤ B0's (medians across runs), and NR frames over 33.3 ms ≤ B0's; NR max ≤ B1 max |
| SC6 | **Low overhead** | NavReady's own game-thread time: median ≤ 0.05 ms per frame, max ≤ 0.50 ms per frame |
| SC7 | **Robust** | Every generated cell reaches AI-ready within 5 s in every NR run; no crash or stuck cell in the edge-case tests (cell cleaned up mid-pipeline, cancelled generation, zero props, navigation flag left on) |

**Rule for SC3–SC5:** an improvement (or "no worse") is claimed only when the run ranges support it (§7). If a criterion is not met, we report it and analyse why. No reframing.

## 7. Measurement plan (summary; the full protocol goes in `09_Testing_And_Benchmarks.md`)

- **Build:** packaged **Development**, standalone, never PIE. The launcher engine can't build Test (UE5-UpdateBudget, Phase 4), and Shipping drops the CSV profiler.
- **Fixed step:** `-deterministic -FPS=60 -novsync`, real GPU, windowed 1280×720, no sound. Every configuration simulates identical frames of game time; CSV `FrameTime`/`GameThreadTime` still record real cost ([01b](01b_Research_Findings.md) §4).
  - **Durations that depend on worker threads** (M1, M2) are measured in **wall-clock** milliseconds, because tile builds and PCG's budget run in real time (`01b` B-1).
- **Capture window:** opened and closed by the game with CSV events (`BenchStart` when the camera starts its path after warm-up, `BenchEnd` when the last agent window closes): `-csvStartOnEvent=BenchStart -csvCaptureOnEventFrameCount=… -ExitAfterCsvProfiling`, plus `-traceautostart=0`.
- **Interleaving:** one discarded priming run per batch, then **5 runs per configuration** (= the number of configurations), with the order rotated each round so every configuration sits in every position exactly once. The baseline batch, captured before NR exists, has 4 configurations and so 4 runs each.
- **CPU pinning:** the batch script pins **itself** once to the P-cores of the hybrid i7-14650HX (`SetProcessAffinityMask`, mask `0xFFFF`) before it records anything or starts a game, and refuses to measure if that fails. It then starts the real game executable (`Build\Windows\NavReadyPCG\Binaries\Win64\NavReadyPCG.exe`, not the bootstrap) with `subprocess.Popen`, which inherits the mask from its first instruction, waits with a timeout and kills the process tree in a `finally` block (`Tools\cpu_affinity.py`, copied from UE5-UpdateBudget; `01b` B-3). The mask read back from Windows is recorded in the environment. `cmd /c start /wait /affinity` is not used: it always returns exit code 0, so a failed run would go unnoticed.
- **Statistics:** per run, median, p95 and p99 (linear-interpolation percentiles) of frame metrics and of per-cell metrics; then the **median across runs with [min–max]**. An improvement is claimed only when the ranges don't overlap.
- **Validity checks before any comparison:**
  1. **B0 must show the problem** (M4 > 0). If it doesn't, the workload is changed (denser props, faster burst) before any NR claim, and the change is recorded.
  2. **Identical content across configurations** (SC2) is also the fairness check.
  3. GPU time is recorded; if it dominates frame time during generation, the scene is lightened.
- **Environment recorded with every batch:** CPU i7-14650HX, GPU RTX 5070 Laptop (driver 610.47), 32 GB, Windows 11 build 26200, project on D: (NVMe), engine on C:, UE 5.8.4, Development, resolution, scalability, power mode, AC power, affinity mask.
- **Raw data:** captures in `Benchmarks\Raw\` (never edited; manifests in git, captures as release attachments), results in `Benchmarks\Summaries\` (see [Benchmarks/README.md](../Benchmarks/README.md)).

## 8. Limits we accept up front

- **Only CPU-spawned, collision-enabled content** is in scope. GPU and componentless output has no collision and no navigation at all, so there is nothing to coordinate (`01b` #2).
- **NR needs the content to be spawned navigation-invisible** (one flag on the spawner's mesh entries). If the flag is left on, NR still gates AI correctly but loses the "one rebuild per cell" benefit (SC7 tests this).
- **One navigation data (one agent size).** Several agent sizes need one readiness check per navmesh (`02` §11).
- **Readiness can be about one frame late** because the "dirty areas queued" check is global (`01b` B-4).
- **Cells are one PCG component each.** A single partitioned component that the scheduler splits into local components is the common production setup; hooking those local components is a growth path, not v1.
- **Single machine, single player.** No claims about other hardware, consoles or networked play.
- **The result may be "no faster".** NR's main promise is correctness (SC1) with no regression (SC3, SC5). If it is also faster, that is a bonus we report only with data.
