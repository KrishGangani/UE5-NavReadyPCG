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
