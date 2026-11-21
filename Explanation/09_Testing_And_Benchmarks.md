# 09 · Testing And Benchmarks: NavReadyPCG

**Rule:** every check is recorded with its date, its exact command and its result, **including failures**. The full functional and benchmark sections are filled in during Phases 5–8. Results from earlier phases are recorded as they happen.

**Every Unreal job runs through the machine-wide queue** (`tools\ue_queue.py run --project P08 …`), which runs one job at a time for all ten portfolio projects and keeps a log per job. The job id is given for each run.

---

## Phase 4 · Foundation checks (2026-11-21)

All six checks run in one queue job: `Tools\phase4_checks.bat` (`05` A8). Toolchain for every build: **MSVC 14.44.35229**, Windows SDK 10.0.22621.0 (UBT log line "Using Visual Studio 14.44.35229 toolchain").

### Run 1: job `20261121_010634_02ba08` (exit 0, 251.6 s)

| # | Check | Command (from `P08_NavReadyPCG\`) | Result |
|---|---|---|---|
| F-1 | Editor target builds | `Tools\build.bat NavReadyPCGEditor Development` | ✅ Succeeded in 63.4 s; 12 actions; **0 errors, 0 warnings**. `UnrealEditor-NavReadyPCG.dll` and `UnrealEditor-NavReadyPCGCore.dll` linked, so the plugin's private `PCG` and `NavigationSystem` dependencies resolve and link in the editor |
| F-2 | Game target builds | `Tools\build.bat NavReadyPCG Development` | ✅ Succeeded in 39.8 s; 24 actions; **0 errors, 0 warnings**. `NavReadyPCG.exe` linked (monolithic, including PCG and the navigation system) |
| F-3 | Uncooked game loads the map, both systems present, quits | `Tools\smoke_game.bat` (`UnrealEditor-Cmd … -game -nullrhi …`) | ✅ Exit 0. `Mounting Project plugin NavReadyPCGCore`; `InternalLoadLibrary: 'NavReadyPCGCore'`; `Template_Default` loaded in 0.098 s; `pcg.FrameTime = "5"`, `pcg.RuntimeGeneration.NumGeneratingComponents = "16"`, `ai.navigation.MaxDirtyAreasCountWhileSuspended = "32768"`, all `LastSetBy: Constructor`. **0 errors.** ⚠️ **22 warnings:** 21 `LogEditorDataStorageUI` (engine-side, from running the editor binary in `-game`; UE5-UpdateBudget saw the same) and **1 `LogEnhancedInput`** ("does not have a valid PlayerInput object"), which is project configuration → fixed (below) |
| F-4 | Package a **Development** build | `Tools\package.bat` | ✅ BUILD SUCCESSFUL; BuildCookRun 73.0 s (cook 34.1 s, stage 35.7 s); 516 packages cooked; cook "Success - 0 error(s), 0 warning(s)"; **0 errors, 0 warnings** in the whole step. Output `Build\Windows\NavReadyPCG.exe` (983 MB staged, git-ignored) |
| F-5 | Packaged build runs without a GPU | `Tools\smoke_packaged.bat nullrhi` | ✅ Exit 0; map loaded in 0.030 s; the same three cvar values, so **PCG and the navigation system are in the packaged build**. **0 errors.** ⚠️ 1 warning: the same `LogEnhancedInput` line |
| F-6 | Packaged build runs on the GPU | `Tools\smoke_packaged.bat gpu` | ✅ Exit 0; D3D12, SM6; **chosen adapter 0 = NVIDIA GeForce RTX 5070 Laptop GPU** (driver 610.47), not the integrated Intel UHD; map loaded in 0.114 s; the same cvar values. **0 errors.** ⚠️ 1 warning: the same `LogEnhancedInput` line. Observation: 35 s passed between the window closing and "Preparing to exit" (first GPU run of a fresh build); checked again in run 2 |

**Fix after run 1:**
1. **The warning.** The engine's default player input class is the legacy `UPlayerInput` (`Runtime/Engine/Private/UserInterface/InputSettings.cpp:52`); the Enhanced Input plugin casts it to `UEnhancedPlayerInput` (`EnhancedInputSubsystems.cpp:79`) and warns when the cast fails (`:118`). Added `Config\DefaultInput.ini` with the two class lines Epic's template ships (`05` A7).
2. **An inert flag.** `-ExitAfterSeconds=0`, copied from UE5-UpdateBudget's smoke script, has no reader anywhere in 5.8.4's `Engine/Source` or `Engine/Plugins/Runtime` (grep, 2026-11-21). The runs exited because of `quit`. Removed from both smoke scripts.
3. **The engine wrote a block into `DefaultEngine.ini`** during F-3 (`AndroidFileServerEditor` settings with a random `SecurityToken`). First left as written; after run 2, on a portfolio-wide rule, the block was deleted and the plugin disabled in the `.uproject`, then checked in run 3 (`05` A1, A7).

### Run 2 (after the fix): job `20261121_011834_043b10` (exit 0, 92.1 s)

| # | Check | Result |
|---|---|---|
| F-1, F-2 | Editor and game builds | ✅ Succeeded, **0 errors, 0 warnings**. Both were up to date (0 actions): only configuration and scripts changed, so run 1's F-1/F-2 remain the compile evidence |
| F-3 | Uncooked smoke | ✅ Exit 0; map in 0.050 s; the three cvar values as in run 1. **0 errors.** The `LogEnhancedInput` warning is **gone**. The remaining 21 warnings are all `LogEditorDataStorageUI: Unable to register widget factory '…' as purpose '0' isn't registered`: editor UI modules that the **editor binary** loads in `-game` mode, not project code, and absent from the packaged runs below |
| F-4 | Package Development | ✅ BUILD SUCCESSFUL; BuildCookRun 70.1 s (cook 31.5 s, stage 35.3 s); 516 packages cooked; **0 errors, 0 warnings** |
| F-5 | Packaged, no GPU | ✅ Exit 0; map in 0.036 s; the three cvar values; **0 errors, 0 warnings** |
| F-6 | Packaged, GPU | ✅ Exit 0; **adapter 0 = NVIDIA GeForce RTX 5070 Laptop GPU**; map in 0.070 s; the three cvar values; **0 errors, 0 warnings**. Shutdown took 0.25 s from the window closing to "Exiting" (run 1's 35 s gap did not recur, so it was a first-run effect of the fresh build, not a project problem) |

### Run 3 (Android File Server disabled): job `20261121_014938_07280f` (exit 0, 118.0 s)

Changed since run 2: `{ "Name": "AndroidFileServer", "Enabled": false }` in the `.uproject`, and the engine-written block deleted from `DefaultEngine.ini` (`05` A1, A7).

| # | Check | Result |
|---|---|---|
| F-1 | Editor build | ✅ Succeeded in 17.2 s; 4 actions (relinked for the changed plugin set); **0 errors, 0 warnings** |
| F-2 | Game build | ✅ Succeeded in 25.9 s; 3 actions; **0 errors, 0 warnings** |
| F-3 | Uncooked smoke | ✅ Exit 0; map in 0.050 s; the three cvar values. **0 errors**; only the 21 engine-side `LogEditorDataStorageUI` warnings of the editor binary (run 2). **`DefaultEngine.ini` was unchanged after this editor run**: no `AndroidFileServer` section reappeared, and the plugin no longer mounts |
| F-4 | Package Development | ✅ BUILD SUCCESSFUL; BuildCookRun 58.8 s (cook 20.9 s, stage 34.7 s); 516 packages cooked; **0 errors, 0 warnings** |
| F-5 | Packaged, no GPU | ✅ Exit 0; map in 0.021 s; the three cvar values; **0 errors, 0 warnings** |
| F-6 | Packaged, GPU | ✅ Exit 0; chosen adapter 0 (the RTX 5070 Laptop GPU, as in runs 1–2); map in 0.052 s; the three cvar values; **0 errors, 0 warnings** |

**Repository history:** the token had only been in local, never-pushed commits. Those commits were folded into one before hand-off, so it appears nowhere in the published history (checked with `git log -p`).

**Phase 4 result:** the editor and game targets build with **0 errors and 0 warnings**, the packaged Development build runs on the RTX GPU with **0 errors and 0 warnings**, and the packaged game contains both PCG and the navigation system.

**What Phase 4 taught us:**
- **Check what the launcher engine can link before designing around it.** Declaring the plugin's `PCG` and `NavigationSystem` dependencies before any code uses them cost nothing and proved, in the first build, that both link into the editor and the packaged game.
- **Treat every warning as a finding.** The one project-side warning came from a missing two-line config file, not from code, and would have repeated in every benchmark log.
- **Check copied flags against the engine you use.** A flag that does nothing is harmless today and misleading tomorrow.

---

## Step S1 · World spike (2026-11-21)

**Question (`03` S1, `01b #8`):** can a world built entirely at runtime, in a **packaged** build, get a navmesh and run PCG's runtime-generation scheduler? **Answer: yes.** The editor-scripted map fallback is not needed.

All runs: packaged Development build, `Tools\s1_checks.bat` through the queue (build both targets → package → `run_packaged.bat … "-NRSpike -NRExit"` without and with the GPU).

### Queue jobs, in order (failures included)

| Job | Result | What it showed |
|---|---|---|
| `20261121_025738_2c007e` | ❌ editor build failed (3 compile errors), **but the job reported exit 0** | `FPCGRuntimeGenerationRadii` members are protected (`C2248` ×2); `SetGraphLocal` needs `PCGGraph.h` (`C2664`). And the check script's `call :step … \|\| exit /b` returned 0 under `cmd /c` (`07`, `05` B6) |
| `20261121_030031_e2fbc3` | ❌ builds ok; spike **FAILED** in both runs, again with exit 0 | Navmesh: **PASS**. Graph wiring failed: the input node has no `"Input"` pin (`LogPCG: Error: From node DefaultInputNode does not have the Input label`). The game's non-forced exit dropped its return code. A `LogCrowdFollowing` warning at every exit |
| `20261121_031954_8afd7b` | ❌ exit 1 (correctly reported now) | Wiring still "failed": `AddLabeledEdge` returns "did the target pin drop edges", not success (`07`) |
| `20261121_034752_6bbeb7` | ✅ exit 0, 132.3 s | **All three checks PASS** in both runs, 0 warnings, 0 errors (table below) |
| `20261121_041759_85fac9` | ❌ editor build failed, **exit 6 reported correctly** (the fixed script) | The simplify pass's `GetNavigation(const UWorld*, …)` helper: `FNavigationSystem::GetCurrent<T>` returns a `const T*` for a const world (`C2440`). Fixed by taking `UWorld*` |
| `20261121_050716_007faa` | ✅ exit 0, 133.4 s | **The final S1 code** (after `code-review`, the simplify pass and the `const` fix): editor build 30.5 s and game build 31.8 s, both **0 errors, 0 warnings**; cook "Success - 0 error(s), 0 warning(s)"; all three checks PASS in both runs; 0 warnings and 0 errors in both game logs |

### The three checks (job `20261121_034752_6bbeb7`; the final job `20261121_050716_007faa` repeated them: path after 0.05 / 0.37 s, generation started at 3,500 / 3,497 cm with 110 instances, carve PASS at 4.01 / 4.02 s)

| # | Check | No GPU (`-nullrhi`) | GPU (RTX 5070, adapter 0) |
|---|---|---|---|
| S1-a | **Runtime navmesh:** a runtime bounds volume (with an added box) → the engine spawns and builds the `RecastNavMesh`; a corner-to-corner `FindPathSync` across the 200 m arena | ✅ result 3 (Success), not partial, 2 points, 268.7 m = the straight line, after 0.06 s | ✅ the same, after 0.35 s |
| S1-b | **Runtime generation:** a runtime-spawned `APCGWorldActor`; the camera starts 55 m from the cell's box and flies in; generation must start inside the 35 m radius | ✅ started at 3,500 cm, 110 instances, generated within one frame | ✅ started at 3,498 cm, 110 instances |
| S1-c | **Props reach navigation uncoordinated:** after generation and 2 idle navigation frames, no navmesh under prop 0, navmesh half-way to the next prop | ✅ under the prop: no; between: yes; ISM navigation-relevant: yes (4.01 s) | ✅ the same (4.02 s) |
| S1-d | Logs | ✅ 0 warnings, 0 errors | ✅ 0 warnings, 0 errors |
| S1-e | Builds | ✅ editor and game: 0 errors, 0 warnings; cook "Success - 0 error(s), 0 warning(s)" | |

**What S1 established, beyond the question:**
1. **Finding 2 of `01b` is now measured**, not only read in the source: CPU-spawned PCG instances with Pawn-blocking collision change the navmesh with no coordination at all (S1-c).
2. A runtime level needs three things the engine doesn't do on its own: navigation bounds with real extent, `RuntimeGeneration=Dynamic`, and an `APCGWorldActor`. The engine does spawn the navmesh itself.
3. **Three ways a check could falsely pass or fail were found and fixed** before any measurement depends on them: the batch exit code (0 for a failed step), the game's exit code (0 for a failed check), and two misread engine return values (`AddEdge`, `AddLabeledEdge`).

### Audit trail for S1

**`code-review` (medium), 10 findings, all fixed** (5 important, 5 nits):

| # | Finding | Fix |
|---|---|---|
| 1 | `if errorlevel 1` misses a crash (NTSTATUS codes are negative) | `if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%`; reproduced: `-1073741819` now stops the script |
| 2 | The navmesh check could read navigation before its first build and lock in a FAIL | Every check is retried until it passes or the run times out |
| 3 | The navmesh check could never run if the build took longer than the camera's 4 s approach | The check no longer depends on the camera |
| 4 | The carve check could run before the new props' deferred octree registration | Retried until it passes (and still waits for 2 idle frames) |
| 5 | The projection's ±200 cm vertical extent could find navmesh on top of a prop | Query 10 cm above the floor, ±20 cm |
| 6 | A failed graph left a deferred, unfinished cell actor in the world | Graph first; `Destroy()` on a failed `Configure` |
| 7 | A set-up failure skipped the summary line | `FinishChecks` logs the summary on every path |
| 8 | A failed radius write left the override on with the unlimited default | `Configure` returns `false`; the override is set only if both writes succeeded |
| 9 | `-Key=Value` arguments were split by batch | Game arguments are one quoted string (`%~3`); tested |
| 10 | 05/06/04 didn't cover the new files | `05` Part B, `06` §7–16, `04` §7–15 |

**Simplify pass** (four angles, done by hand: this project runs no sub-agents), 6 changes, none changing behaviour: one `GetGeneratedComponents` helper instead of two copies of the tag filter (reuse); one `GetNavigation` lookup instead of three (reuse); `enum class EVerdict` instead of `0/1/-1` (simplification); `AllChecksPassed()` used instead of a duplicate expression; the unreachable "PENDING" text removed; the floor-scale expression named once. Efficiency: nothing material (the per-frame retries stop at the first pass and only run in the spike). Altitude: the fixes sit at the right depth (forced exit only on failure because Windows drops the code; reflection because the members are protected; config to remove the crowd manager).

---

## Step S2 · Content and the determinism hash (2026-11-21)

**Question (`03` S2, SC2):** does the same seed give the same content, run to run, with and without the GPU, cooked and uncooked; does another seed change every cell; and does the layout leave every open floor point reachable? **Answer: yes, after one calibration.** The first run found one enclosed sample point; narrowing the jitter removed it (below). The final job passes every check.

All runs: `Tools\s2_checks.bat` through the queue (build both targets → `run_tests.bat NavReadyPCG.Hash` → package → five runs with `run_game.bat … "-NRRun -NRSeed=… -NRRunTag=… -NRExit"`: seed 1 twice, seed 2, seed 1 on the GPU, seed 1 uncooked → `compare_hashes.py`).

### Queue jobs, in order

| Job | Result | What it showed |
|---|---|---|
| `20261121_055827_38d320` | ❌ exit 1 (correctly: the comparison failed), 192.5 s | Builds 0 errors / 0 warnings (editor 21.6 s, game 19.6 s); the 5 hash tests pass; cook "Success - 0 error(s), 0 warning(s)", 526 packages. **Determinism: PASS** (table below). **Pocket check: FAIL**: one sample point in cell 15 enclosed, in all four seed-1 runs (seed 2: none). Jitter was ±50 cm |
| `20261121_070934_305b46` | ✅ exit 0, 204.8 s | **The final S2 code** (jitter ±25 cm, pocket points logged): builds 0 errors / 0 warnings (editor 23.6 s, game 26.4 s); 5 of 5 hash tests; cook 0 / 0, 526 packages; **0 pocket points in every run**; determinism PASS again (seed 1 run hash `0x8CEE4992` in all four runs, seed 2 `0x5C3BCDAF`, 16 of 16 cells differ); `S2 check: PASS` |

### The checks (first job; the final job repeated S2-a to S2-h and S2-j with the same outcome, and passed S2-i)

| # | Check | Result |
|---|---|---|
| S2-a | Builds | ✅ editor and game: 0 errors, 0 warnings |
| S2-b | Unit tests `NavReadyPCG.Hash.*` | ✅ 5 of 5 (`Found 5 automation tests`; `Empty`, `IgnoresFloatNoise`, `OrderIndependent`, `QuaternionSign`, `SeesRealChanges`: Success; exit 0) |
| S2-c | Package | ✅ 0 errors, 0 warnings |
| S2-d | Layout facts logged by the director | ✅ both shapes span z −50 to 50 cm around their pivot (the centre: the scale range's premise); 988 cm tiles from the world origin, aligned with the cells; 16/16 cells with props that block navigation |
| S2-e | Content | ✅ 900 instances in every run; 49 / 56 / 64 per cell (mean 56.2), as the lattice predicts |
| S2-f | **Same seed, same content** | ✅ seed 1: run a = run b = GPU = uncooked in **16 of 16 cells** (run hash `0x29449002` in all four) |
| S2-g | **Another seed changes every cell** | ✅ seed 2: content differs in **16 of 16 cells** (run hash `0xA7EE575A`) |
| S2-h | Instance order (H6) | Observed: the order-dependent hash also matched in all 16 cells of all four seed-1 runs, so PCG produced the same instance order every time here. Reported, not required |
| S2-i | **No pockets** | First job ❌: cell 15, 1 of 156 open sample points unreachable, in every seed-1 run; seed 2: 0. **Final job ✅: 0 in every run** (149 to 167 open points per cell of 196) |
| S2-j | Logs | ✅ packaged runs: 0 warnings, 0 errors. Uncooked: 0 errors and the 21 engine-side `LogEditorDataStorageUI` warnings of the editor binary (as in Phase 4, F-3) |

**Timing facts** (functional runs; no timing claims are made from them): the navmesh was complete 0.05–0.28 s after begin play; a cell generated in 2–15 ms from start to done; the pocket check (about 2,500 path queries) took 0.18–0.21 s.

### What the first run taught us

1. **A layout can enclose a pocket even when every gap is wider than the agent.** With ±50 cm of jitter the narrowest gap is about 1 m, wider than the 68 cm agent, but the navmesh closes gaps narrower than about 1.15 m: the 34 cm radius is rounded up to two 19 cm voxels on each side, and rasterization can add up to a voxel more on each side **[inferred]**. One sample point in one cell of one seed was enclosed. **Change:** jitter ±25 cm, so the narrowest gap is about 1.5 m (`02` §14, `05` C3). The run's own check found it; the agents in S4 would otherwise have reported it as path failures.
2. **Without the GPU, game time runs ahead of the wall clock.** The same path in game time put the last cell's generation at 8.2 s without the GPU, 11.4 s uncooked, and 20.1 s on the GPU (where the path predicts 20.09 s). Without rendering a frame takes well under 0.5 ms, and the world clamps each frame's game time to at least `MinUndilatedFrameTime` = 0.5 ms (`BaseGame.ini:198`, `WorldSettings.cpp:334–343`, `LevelTick.cpp:1599`). So `-nullrhi` runs are for functional checks only; every measurement runs on the GPU (`07`).
3. **The content hash is stable across builds and RHIs**, and so is the instance order in this workload.

### Audit trail for S2

**`code-review` (medium), 9 findings: 8 fixed, 1 refuted** (4 important, 5 nits). Review of the working-tree diff against `474d40f`, before the first S2 build:

| # | Finding | Outcome |
|---|---|---|
| 1 | `run_game.bat`: in `uncooked` mode, `exit /b %ERRORLEVEL%` sat inside an `if ( … )` block, which cmd expands when it reads the block, so a failed uncooked run returned 0; with no editor start, `compare_hashes.py` could read an old log | **Fixed:** each launch on its own line (`goto :uncooked`); the old log is deleted first. Reproduced: 0 inside a block, 3 with the `goto` (`07`) |
| 2 | Props too low to block: centre-pivot shapes on the floor stand 50 cm × scale tall, so at scale 0.6 (30 cm) they are under the 35 cm step height, and their tops fall inside the pocket query band | **Fixed:** scale 0.9–1.4 (45–70 cm); jitter ±0.5 m so every gap stays ≥ about 1 m; the director logs the meshes' bounds so the pivot is a recorded fact (`02` §14, `05` C3) |
| 3 | The pocket check trusted one idle navigation frame (S1 required two) | **Fixed:** `RequiredIdleFrames = 2` in a row |
| 4 | A run passed even if no prop reached navigation (S1's carve check was gone) | **Fixed:** the run also needs props in every cell and at least one blocked sample point per cell |
| 5 | Delegates were bound, and `FinishSpawning` called, before the cell's record existed | **Fixed:** record and cell first |
| 6 | A cell generating before the flight would get a time measured from 0 (boot) | **Fixed:** it is logged as an error and fails the run |
| 7 | `compare_hashes.py` crashed on a missing log; an empty reference gave a misleading message | **Fixed:** reported as failures; self-test re-run |
| 8 | `run_tests.bat` would pass if the filter matched no test | **Refuted by the source:** zero matches is an error unless a command allows it (`bAllowZeroTestResults` defaults to false, `AutomationCommandline.cpp:346–355, 868`), and errors make `Quit` exit −1 (`:491–504`) |
| 9 | `-NRSeed` could overflow `BaseSeed × 100`; `-NRRunTag` could write outside `Saved/NRRuns` | **Fixed:** seed clamped to 0–1,000,000; tag through `FPaths::MakeValidFileName` |

**Simplify pass** (four angles, done by hand: this project runs no sub-agents): one `NRBench::ArenaCorner()` instead of two copies of the corner arithmetic (reuse); the header's command-line notes brought up to date. Efficiency: the pocket check runs about 2,500 path queries once, after the measurement, in about 0.2 s (above). Altitude: global idleness is used only by the director at the end of a run; per-cell readiness belongs to the plugin (S3), which must not use it (`07`).

---

## Step S3 · The readiness observer and the measurement harness (2026-11-21)

**Question (`03` S3):** does NavReady's probe report each cell's "tiles rebuilt" moment correctly, in time to measure M1 for every cell of a B0 run, with the CSV columns the comparison needs? **Answer:** yes for B0 (16 of 16 cells reach ready in every run, M1 measured per cell, all CSV columns present). The one-cell probe-vs-engine agreement check was not run (project closed early).

All runs: `Tools\s3_checks.bat` through the queue (build both targets → every `NavReadyPCG.*` test → package → `csv_stats.py` and `check_probe.py` self-tests → `run_bench.py --configs B0 --runs 1` (priming + 1 run, pinned, real time at 60 fps, real GPU) → `run_bench.py --cells 1 --no-prime` → `check_probe.py` on the one-cell capture).

### Queue jobs, in order (failures included)

| Job | Result | What it showed |
|---|---|---|
| `20261121_083650_13fc7a` | ❌ editor build failed, exit 6 (correctly reported) | `RecastNavMeshGenerator.h(21): fatal error C1083: Cannot open include file: 'Recast/Recast.h'`: the Recast headers belong to the `Navmesh` module, which `NavigationSystem` depends on privately. Fixed by adding `Navmesh` to the plugin's private dependencies (`05` D3b, `07`) |
| *(retry, cancelled when the project was closed early)* | — | Not run; the S3 code was built and run in the final job instead (below) |

### Audit trail for S3

**`code-review` (medium), 10 findings, all fixed** (3 important, 7 nits). Review of the working-tree diff against `1b217ff`, before the first S3 build:

| # | Finding | Fix |
|---|---|---|
| 1 | `s3_checks.bat`'s `:step` passed `%2 … %9`: eight arguments, so the benchmark steps lost their last options (`--label`, `--cells 1`, `--no-prime`) and the probe check would have found no capture | Arguments collected with `shift` in a loop; tested with ten arguments, one quoted, and with a failing command (`07`) |
| 2 | `check_probe.py` ignored tile tasks already running in the commit frame, and could call a correct run inconclusive | The commit frame counts; a self-test case for it |
| 3 | The readiness bounds used `GrowBoundingBox` (low, high), while the engine grows dirty areas by (high, low): narrower on one side if the two differ | The larger growth per axis on both sides (`05` D3) |
| 4 | `HasDirtyTiles(FBox)` doesn't see a tile in the middle of a time-sliced rebuild | `static_assert(RECAST_ASYNC_REBUILDING)`: time slicing only happens without asynchronous rebuilding, the default (`RecastNavMesh.h:28–32`) |
| 5 | A cell seen generated while no generator existed got ungrown bounds for good | It waits in `Generated` until a generator exists |
| 6 | The CSV events used the registry index, which moves when a cell unregisters | A stable `Id` per cell, given at registration |
| 7 | A run that never started its flight still logged `BenchEnd` and converted NavReady times from 0 | Both only if the flight started; otherwise −1 |
| 8 | `run_bench.py` accepted `--cells` outside 1–16 and labels with spaces | Both rejected |
| 9 | The S3 code had no matching `Explanation/` updates yet | `05` Part D, `06` §24–29, `04` §23–27, `07`, `01`/`02`/`03`, `Benchmarks/README.md` in this step |
| 10 | A late "generated" event after a cancel moved an idle cell to `Generated` | `OnGenerated` only from `Generating`; two test assertions |

**Simplify pass** (by hand): nothing removed. The subsystem's per-frame loop already reads only plain data; the two "ready" counts in the director (live components for the settle wait, copied records for the pass rule) answer different questions, so both stay. Efficiency: `HasDirtyTiles(FBox)` walks the pending and running tile lists for each committed or ready cell (16 cells × at most a few hundred tiles per frame); the cost is measured as `Exclusive/GameThread/NavReady` (SC6). Altitude: the probe lives in the plugin, the director only reads it.

---

## Final step · NavReady in its smallest working form, and the short batch (2026-11-21)

**Owner's direction:** finish now: the core feature in the game, one short batch (B0 vs NR, 2 runs + priming), no further campaigns, no code review.

| Job | Result | What it showed |
|---|---|---|
| `20261121_111500_859557` | ❌ exit 2, 116.7 s | Builds ✅ (0 errors, 0 warnings); **9 of 9 automation tests pass** (5 hash + 4 cell); package ✅ (cook 0 errors / 0 warnings). The batch didn't start: an unquoted `B0,NR` was split by cmd at the comma (`07`) |
| `20261121_111727_dfe693` | ❌ exit 1 | Builds, 9/9 tests and package ✅ again. Batch `2026-11-21_111841_final`: **B0 ✅ in both runs; NR ❌ in both runs**: every cell generated and was hashed (the hash at AI-ready matched the generated content in 16/16 cells), but **0 of 16 cells had props that block navigation**: switching an already populated ISM to navigation-relevant registers it without dirtying its area (`06` §30). Fix written: the commit calls `AddDirtyArea`; verified below |

**The batch** (pinned 0xFFFF, real time at 60 fps, RTX 5070 Laptop, 1280×720; raw files `Benchmarks/Raw/2026-11-21_111841_final_*`, summary `Benchmarks/Summaries/2026-11-21_111841_final_summary.md`):

| Metric | B0 (2 runs: median [min–max]) | NR |
|---|---|---|
| M1 time to navigable per cell, median | **20.1 ms** [19.7–20.5] | invalid (nothing was rebuilt) |
| M1 p95 | **28.5 ms** [25.9–31.2] | invalid |
| Settle (generated → ready), median | 12.9 ms [12.6–13.3] | invalid |
| `GameThreadTime` p99 | 2.7 ms [2.2–3.3] | 2.2–3.0 ms (but no navmesh work) |
| `GameThreadTime` max | 237 ms [227–248]: **includes the end-of-run pocket check** (≈ 0.2 s, inside the capture), so M3's worst frame is not measured cleanly | 32–36 ms |
| NavReady's own cost (`Exclusive/GameThread/NavReady`) | median 0.003 ms, max 0.19–0.22 ms per frame | median 0.003 ms, max 0.23–0.33 ms |
| Ready cells re-dirtied by a neighbour's edge props | 32 and 35 episodes per run | 0 (invalid) |
| Content hash | identical in every run (`0x8CEE4992`) | identical to B0 (`0x8CEE4992`): the navigation flag doesn't change the content |

### Verification of the fix (one more job, on the coordinator's request)

| Job | Result | What it showed |
|---|---|---|
| `20261121_112413_1c329d` | ⚠ exit 143, **not a test result** | Builds, 9/9 tests, package and the first two B0 runs passed; then my own shell's 175 s timeout killed the queue client, and the queue stopped the job during the first NR run. Re-run unchanged |
| `20261121_112721_9a0628` | ✅ exit 0 | Builds 0 errors / 0 warnings; **9/9 tests**; package 0 / 0; batch `2026-11-21_112845_final`: **all 5 runs pass, NR included**: in both NR runs 16/16 cells have props that block the navmesh, 16/16 reach ready, and NavReady's hash at ready matches the generated content in 16/16 cells |

**The batch** (pinned 0xFFFF, real time at 60 fps, RTX 5070 Laptop, 1280×720; raw `Benchmarks/Raw/2026-11-21_112845_final_*`, summary `Benchmarks/Summaries/2026-11-21_112845_final_summary.md`). Each value: median across the 2 runs [min–max]:

| Metric | B0 | NR |
|---|---|---|
| M1 time to navigable per cell, median | **20.4 ms** [19.1–21.8] | **36.1 ms** [34.4–37.9] |
| M1 p95 | 30.1 ms [25.7–34.5] | 41.1 ms [40.2–42.1] |
| Settle (generated → ready), median | 12.7 ms [11.4–14.0] | 28.3 ms [27.3–29.2] |
| `GameThreadTime` p99 | 2.8 ms [2.4–3.2] | **2.3 ms** [2.3–2.3] |
| `GameThreadTime` max | 243 ms (includes the end-of-run pocket check, ≈ 0.2 s, inside the capture) | 253 ms (same) |
| `Exclusive/GameThread/NavigationBuild` max | 3.9–4.7 ms | 2.6–2.9 ms |
| NavReady's own cost | median 0.0024–0.0027 ms, max 0.31–0.45 ms | median 0.0025–0.0027 ms, max 0.30–0.44 ms |
| Ready cells re-dirtied by a neighbour's edge props | 32, 35 | 38, 38 |
| Content hash (seed 1) | `0x8CEE4992` | `0x8CEE4992` |

**Reading it honestly.** NR is **slower** to navigable: about +16 ms per cell at the median. That is about one frame at 60 fps (16.7 ms), which fits NR's design **[inferred]**: B0's props dirty their tiles while PCG spawns them, but NR's commit runs in the NavReady tick after the "generated" event, the dirty areas are flushed on a later navigation tick, and in a burst the one-cell-per-frame budget queues cells. In exchange, NR's frames were cheaper: game-thread p99 2.3 ms vs 2.8 ms with non-overlapping run ranges, and a lower navigation-build peak. That is what one rebuild per cell instead of incremental rebuilds during generation would give **[inferred]**. With 2 runs per configuration this is a first reading, not a verdict.

## Success criteria: what the data shows

| # | Criterion | Status |
|---|---|---|
| SC1 | AI never paths through unready content | **not measured** (no measuring agents) |
| SC2 | Determinism | ✅ **met** for what was run: same seed → same hash in 16/16 cells across runs, GPU and no GPU, cooked and uncooked (S2), and in B0 and NR; another seed changes 16/16; NavReady's hash at ready = the generated content in 16/16 cells |
| SC3 | NR median M1 ≤ 1.10 × B0, NR p95 ≤ B0 p95 | ❌ **not met**: 36.1 vs 20.4 ms median (1.77×), 41.1 vs 30.1 ms p95. The B1 part was not measured |
| SC4 | AI activates promptly | **not measured** |
| SC5 | No worse worst frame | **not measured** cleanly: the max frame of both is the pocket check. p99 `GameThreadTime` is lower in NR (2.3 vs 2.8 ms, ranges don't overlap) |
| SC6 | NavReady median ≤ 0.05 ms, max ≤ 0.50 ms per frame | ✅ **met** in all runs: median ≤ 0.003 ms, max ≤ 0.45 ms |
| SC7 | Robust | partly: 16/16 cells reached ready in every run of both configurations; the edge-case tests were not written |
