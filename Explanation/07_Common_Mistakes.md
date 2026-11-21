# 07 · Common Mistakes: NavReadyPCG

**Rule:** every mistake listed here was either made and caught in this project, or found in the engine source as a trap the design must avoid. Each has a way to spot it and a way to prevent it. Module-by-module entries (lifetime, delegates, ticking, GC) are added with the code in S1–S5.

---

## Build and project set-up (Phase 4: caught by the checks)

| Mistake | How to spot it | How to prevent it |
|---|---|---|
| Leaving the engine's legacy `UPlayerInput` as the default while Enhanced Input is enabled | `LogEnhancedInput: Warning: … does not have a valid PlayerInput object. Failed to load user settings.` on every map load (`09` F-3, F-5, F-6, first run) | `DefaultInput.ini`: `DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput` and the matching component class (`05` A7) |
| Committing the engine-written `AndroidFileServerRuntimeSettings` block (a random `SecurityToken`) from `DefaultEngine.ini` | `git diff` shows a section you never wrote, appearing after the first editor run | Disable the plugin in the `.uproject` (`{ "Name": "AndroidFileServer", "Enabled": false }`), delete the block, and check the file after the next editor run (`05` A1, A7; `09` run 3) |
| Copying a command-line flag the engine doesn't read (`-ExitAfterSeconds=0`) | The run still exits, but only because of `quit`; a grep of the engine source finds no reader | Check every flag against the engine version before relying on it (`05` A8) |
| A plugin module depending on another plugin's module without listing that plugin in the `.uplugin` | UBT warning "Plugin '…' does not list plugin '…' as a dependency …" (`UEBuildTarget.cs:1578`) | `"Plugins": [ { "Name": "PCG", "Enabled": true } ]` in the plugin descriptor (`06` §5) |
| Two modules with the same name (e.g. calling the plugin `NavReadyPCG` like the game module) | UBT silently keeps one of them | A unique name: `NavReadyPCGCore` ([research/04](Research/04_local_engine_verification.md) §0) |
| Planning on the **Test** configuration with a launcher engine | "Targets cannot be built in the Test configuration with this engine distribution" | Headline builds are packaged Development (found by UE5-UpdateBudget) |
| `sed -i` on a Windows batch file in Git Bash | `file` reports "ASCII text" instead of "with CRLF line terminators" | Re-apply CRLF after any scripted edit; `.gitattributes` keeps `*.bat` as CRLF in the repository |
| `call :step … \|\| exit /b` to stop a batch script at the first failure (found in S1) | The queue reports **exit 0** for a job whose build failed: under `cmd /c`, `exit /b` without a number returned 0 (reproduced: 0 instead of 6) | `if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%` on the line after each `call` (`05` B6). Not `if errorlevel 1`: it means "1 or more", so a crash's negative code (`-1073741819`) passes it. Never trust a job's exit code until a deliberately failing step has been seen to fail it |

## Using PCG from C++ (S1)

| Mistake | How to spot it | How to prevent it |
|---|---|---|
| Creating a node whose settings class isn't exported (e.g. `UPCGCreatePointsGridSettings`, plain `UCLASS(BlueprintType)`) | Linker error on `StaticClass()` | Use exported (`MinimalAPI`) settings; the rock field samples the cell's box with `UPCGVolumeSamplerSettings` (`05` B1) |
| Writing `FPCGRuntimeGenerationRadii` members directly | `error C2248: cannot access protected member` | They are `BlueprintReadWrite` UPROPERTYs: write them through reflection, then call `ComputeHash()` (`05` B2) |
| Using an engine struct's `StaticStruct()` when the struct isn't exported (`FPerQualityLevelFloat`) | Linker error | Compare the reflected struct by name (`05` B2) |
| Trusting `UPCGGraph::AddEdge` or `AddLabeledEdge` to report success | `AddEdge` always returns the target node; `AddLabeledEdge` returns "did the target pin drop other edges" (`PCGGraph.h:504`), so it is `false` both on success and on a missing pin. Made in S1: a correct graph was reported as "could not wire" | Check the target pins with `UPCGNode::IsInputPinConnected` (`05` B1) |
| Using `PCGInputOutputConstants::DefaultInputLabel` ("Input") as the graph input node's pin | `LogPCG: Error: From node DefaultInputNode does not have the Input label` | The input node's static pin is `PCGPinConstants::DefaultInputLabel` ("In", `PCGInputOutputSettings.cpp:27–36`) |
| Passing a `UPCGGraph*` where `UPCGGraphInterface*` is expected with only a forward declaration | `cannot convert argument 1 from 'UPCGGraph *' to 'UPCGGraphInterface *'` | Include `PCGGraph.h`: the compiler needs the full class to know the inheritance |
| Leaving a runtime component's generation radius at the default | Every cell generates at once: the default `RadiusUnbounded` is `(1u << 31) × 2` cm | `bOverrideGenerationRadii = true` and a real radius (`05` B2) |
| Expecting runtime generation in a map without a saved PCG world actor | Nothing generates; the scheduler only ticks `if (PCGWorldActor)` | Spawn an `APCGWorldActor` at runtime (`05` B3) |

## Engine traps the design avoids (found in Phase 2)

| Trap | What goes wrong | How the design avoids it | Evidence |
|---|---|---|---|
| Releasing a navigation build lock with the **default** argument | `RemoveNavigationBuildLock(Flags)` uses `Rebuild`, which calls `RebuildAll()`: the whole navmesh is discarded and rebuilt, and every agent in the world loses navigation until it is back | NavReady never locks. If a lock is ever needed, release it with `ELockRemovalRebuildAction::NoRebuild`. Baseline B1 reproduces the trap on purpose | `01b #3` |
| Following the documented `ReleaseInitialBuildingLock()` pattern | Same full rebuild in 5.8.4. The docs' tip to reset the dirty areas first can't be followed without a subclass (the member is protected) | as above | `01b` §5 |
| Treating a lock as per-region | It is global: a region still generating blocks every finished region | Per-cell commits (`02` §2) | `01b #4` |
| Assuming PCG output never affects navigation | CPU-spawned ISMs with Pawn-blocking collision **do** update navigation on every `AddInstances` | The content is spawned navigation-invisible and committed once per cell | `01b #2` |
| Waiting for `IsNavigationBuildInProgress() == false` or `OnNavigationGenerationFinished` before activating AI | Both are global; under continuous generation they arrive late or never | Per-cell readiness with `HasDirtyTiles(CellBounds)` (baseline B3 measures the global gate) | `01b #6` |
| Declaring a cell ready the same frame its content is handed to navigation | Registration is deferred to the next navigation tick, so "no dirty tiles" is trivially true before navigation has seen anything | Readiness requires at least one navigation tick after the commit | `01b #7` |
| Caching the navmesh generator pointer | A full rebuild can recreate the generator | Fetch it every frame | `NavigationData.cpp:626` |
| Budgeting PCG with `pcg.FrameTime` alone | The runtime-generation scheduler and the navigation work it causes are outside that budget | Measure `Tickables`, `NavigationBuild` and NavReady's own stat separately (`01` §6.3 M6) | `01b #11` |
| Expecting GPU-spawned PCG instances to block AI | GPU and componentless instances have no collision and no navigation | Out of scope; use CPU spawning (or navigation proxies) for anything AI must respect | `01b #2` |
| Measuring "time-to-navigable" in game time under `-deterministic` | Tile builds run in real time on worker threads; fixed-step game time says nothing about them | Wall-clock timestamps (`FPlatformTime::Seconds()`) | `01b` B-1 |
| Launching timed runs with `cmd /c start /wait /affinity` | The exit code is always 0, so a crashed run looks fine | Pin the batch script itself once (`pin_self`), then `subprocess.Popen` + timeout + kill in `finally`; children inherit the mask (`05` A9) | UE5-TestTrust |
