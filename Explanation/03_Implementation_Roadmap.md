# 03 · Implementation Roadmap: NavReadyPCG

**Written:** end of Phase 3, 2026-11-21 · **Engine:** UE 5.8.4

**Rules for every step:**
- Each step is small enough to read in one sitting: a few hundred lines at most.
- A step is finished only when its code **compiles with 0 warnings through the machine-wide queue**, its check **passes**, and its `Explanation/` entries (`05`, `06`, and `04`/`07` where relevant) are **written in the same step**.
- After each step that adds C++: run `simplify`, then `code-review`.
- **The baselines are captured before the pipeline exists.** Steps S1–S4 build the world, the content, the measurement harness, the agents and the baselines B0–B3, and S4 ends with the baseline capture. The NavReady commit and AI gating start at S5.
- Every Unreal job (build, package, editor, game run, benchmark) goes through `tools\ue_queue.py`.

---

**Progress (2026-11-21):** Phase 4 ✅ (F1–F3, checks F-1 to F-6 in `09`). S1 ✅ (checks S1-a to S1-e in `09`). S2 ✅ (checks S2-a to S2-j in `09`). Next: **S3**.

## Phase 4: Foundation (no gameplay code yet) ✅

| Step | What gets built | How it is checked |
|---|---|---|
| **F1** ✅ | `NavReadyPCG\NavReadyPCG.uproject` (EngineAssociation 5.8, plugins `NavReadyPCGCore` and `PCG` enabled, `AndroidFileServer` disabled), `Source\NavReadyPCG.Target.cs`, `NavReadyPCGEditor.Target.cs`, the game module (`NavReadyPCG.Build.cs`, `NavReadyPCGModule.cpp`) | `Tools\build.bat` for the editor and the game target: 0 errors, 0 warnings (`09` F-1, F-2) |
| **F2** ✅ | The plugin skeleton: `NavReadyPCGCore.uplugin` (declares its dependency on the `PCG` plugin), `NavReadyPCGCore.Build.cs` with **private `PCG` and `NavigationSystem` dependencies declared before any code uses them**, the module class | Same builds. The point is to prove early that a launcher engine links PCG and the navigation system into both targets (the brief's "check early" rule) |
| **F3** ✅ | `Config\DefaultEngine.ini` (default map, no smoothing, no VSync), `DefaultGame.ini`, `DefaultInput.ini` (added after F-3/F-5 found a warning), `Tools\build.bat`, `package.bat`, `smoke_game.bat`, `smoke_packaged.bat`, `phase4_checks.bat`, `cpu_affinity.py` | Uncooked smoke, packaged Development build, packaged smoke without and with the GPU; each prints `pcg.FrameTime`, `pcg.RuntimeGeneration.NumGeneratingComponents` and `ai.navigation.MaxDirtyAreasCountWhileSuspended`, proving both systems are in the build (`09` F-3 to F-6) |

## Phase 5: Implementation

| Step | What gets built | How it is checked | Explanation files touched |
|---|---|---|---|
| **S1. World spike (de-risking)** ✅ | `UNRBenchmarkSubsystem` (first version): spawns the floor, navigation bounds (an `ANavMeshBoundsVolume` given extent by an added box component, because a runtime brush has no geometry: `02` §12 #7) and an `APCGWorldActor` at runtime; `DefaultEngine.ini` gets `RuntimeGeneration=Dynamic` for the Recast navmesh; one test PCG component with `GenerateAtRuntime` | In the **packaged** build: the navmesh builds across the arena (a `FindPathSync` across it succeeds), and the runtime-generation scheduler generates the test component when the camera comes in range. **If either fails**, switch to the fallback (an editor Python commandlet that writes the map, run through the queue) and record why (`01b #8`) | 02 (if changed), 05, 06, 07, 09 |
| **S2. Content + determinism hash** ✅ | `NRGraphFactory` extended (Volume Sampler → Transform Points → Static Mesh Spawner; no thinning node, `02` §14; collision `BlockAll`, navigation flag per configuration); `ANRCellActor`; 16 cells and the camera path (cruise, burst); `NavReadyContentHash` in the plugin (pure functions); run report with per-cell hashes; cells on navmesh tile boundaries; **pocket check** (prop density) | Unit tests of the hash on hand-made transforms (order independence, quantization edges). Same seed twice → identical 16 hashes; seed + 1 → all 16 differ; editor build = packaged build. Every run: zero pocket points; 49 to 64 props per cell (`01` §6.1) | 04, 05, 06, 09 |
| **S3. Measurement harness + readiness observer** | `FNavReadyCell` + `FNavReadyProbe` (pure policy, observe-only transitions), `UNavReadyCellComponent`, `UNavReadySubsystem` (registry, probe, M1 timestamps, CSV `NavReady/*`; **no commits yet**); `Tools\csv_stats.py` and `Tools\run_bench.py` adapted from UE5-UpdateBudget (interleaving, P-core pinning via `cpu_affinity.py`, manifests); `CSV_EVENT(BenchStart/BenchEnd)` | `csv_stats.py --selftest`. One B0 run produces M1 for all 16 cells and a CSV with `Exclusive/GameThread/{Tickables,NavigationBuild,NavReady}` columns. The probe's "ready" moment agrees with `NavTasks/NumRemainingTasks` reaching 0 when only one cell is building | 04, 05, 06, 08, 09 |
| **S4. Agents, path checks, baselines → baseline capture** | `ANRAgent` (dormant/active, `FindPathSync`, kinematic follow, F1–F3 checks, settled re-check); director activation policy; `UNRBaselineSubsystem` (B1/B2 locks, B3 gate) | Functional: B1 shows `RebuildAll` in the log after each unlock; B2 doesn't. **Validity check:** B0 shows M4 > 0 (else the workload is changed and the change recorded). **The baseline capture:** B0–B3 × 4 interleaved runs + 1 priming run, fixed step, packaged, pinned, into `Benchmarks\Raw\`. H1–H5 answered with data **before any pipeline code** | 05, 06, 07, 09 |
| **S5. The pipeline + tests** | `FNavReadyCell` full transitions; the commit queue (`nr.CommitsPerFrame`) and `SetCanEverAffectNavigation(true)`; settle → hash → `OnCellStateChanged(AIReady)`; the director activates agents on AI-ready (NR); the "flag left on" warning | Automation tests with fake probes: never ready before one navigation tick; never ready while locked, while areas are queued or while the cell has dirty tiles; reset from every state; regeneration. Functional tests: a cell cleaned up mid-pipeline, a cancelled generation, a cell with zero props, the flag left on. A first NR run: SC1 and SC7 checked on one run before the full matrix | 04, 05, 06, 07, 09 |
| **S6. Telemetry + debug view** | CSV `NavReady/*` complete; per-cell tile counts (`GetDirtyTilesCount` sampled per frame, for M7/H2); `nr.Debug` draw (cell bounds tinted by state, agent paths, red on F3) | The debug view in a packaged run (screenshot for `Media\`); the CSV columns present in a capture | 05, 06, 08 |
| **S7. (Optional) commit-budget sweep hook** | `nr.CommitsPerFrame` read from the command line by the director for sweeps | A sweep of 1 / 2 / 4 / 16 runs end to end | 05, 09 |

## Phases 6–10 (after the code exists)

| Phase | Work |
|---|---|
| 6 Explanation | Consolidate 04–07 into one consistent line-by-line pass. Re-check every API against 5.8.4 |
| 7 Testing | Edge cases (0 and 1 cells; all cells generating at once; `nr.CommitsPerFrame` 0 is rejected; level unload mid-pipeline; navmesh missing), stability (a 30-minute soak with repeated generate/cleanup), scalability (64 cells) |
| 8 Profiling | The full matrix: B0–B3 + NR × 5 interleaved runs + priming, fixed step, pinned; the commit sweep; one uncapped real-time confirmation run; Insights drill-downs of the PCG executor vs scheduler split (H5). Optimisations only with data (`11`) |
| 9 Code review | `code-review` and `simplify`, plus a written self-review: weaknesses, not praise |
| 10 Portfolio | Charts (`dataviz`), an architecture page, the demo video, README results, interview Q&A |

**Rough matrix cost:** 26 runs × (about 20 s load + 5 s warm-up + about 70 s path) ≈ 45 minutes per batch, scripted and unattended, queued behind the other projects' jobs.
