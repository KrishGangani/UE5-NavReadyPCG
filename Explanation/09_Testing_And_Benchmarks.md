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

## Phase 5 onwards

*(Each step's checks are recorded here as they run: S1 world spike, S2 content and determinism, S3 harness, S4 baselines and the baseline capture, S5 pipeline tests, S6 telemetry.)*
