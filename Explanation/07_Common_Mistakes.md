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

## Content, hashing and layout (S2)

| Mistake | How to spot it | How to prevent it |
|---|---|---|
| Taking `TileSizeUU` (1000) as the navmesh tile size | Cells planned as "3 × 3 tiles of 10 m" straddle tile edges; per-cell tile counts don't add up | The real tile is `TruncToInt(TileSizeUU / CellSize) × CellSize` = 988 cm with the defaults (`ARecastNavMesh::GetTileSizeUU`). Lay cells out from that value and check it against the live navmesh (`05` C4) |
| Putting a cell edge on the Volume Sampler's lattice | Two overlapping props on the shared edge, one from each cell; more instances than the lattice predicts | The lattice is world-aligned (multiples of the voxel size), so keep every cell edge off it; S2's first layout had the edge y = 0 on it (`05` C3, C4) |
| `FCrc::StrCrc32(View.GetData(), View.Len())` | Compiles; the hash changes with unrelated memory after the view | The second parameter is a seed. Use `StrCrc32Len(Data, Length)` (`06` §17) |
| Hashing a struct's raw memory | Equal values hash differently between runs or builds | Padding bytes are undefined. Hash an array of one type, or each member (`04` §16) |
| Hashing PCG content in generation order | The hash changes between runs although the content didn't, if instances come out in another order (H6) | Sort per-instance hashes before combining; keep the ordered hash only as a report (`05` C1) |
| `TestEqual(TEXT("…"), A, B)` with two `uint32` | A compile error: the call is ambiguous between the `int32`, `int64`, `SIZE_T`, `float` and `double` overloads (`AutomationTest.h:1985–1989`; found reading the header) | Cast to `int64` (`06` §18) |
| Using global navigation idleness (`!IsNavigationBuildInProgress() && !HasDirtyAreasQueued()`) as "this cell is ready" | Correct only when nothing else is building; with cells generating in a burst it waits for all of them | Only for a settled world (end of a run). Per-cell readiness is `HasDirtyTiles(CellBounds)` (S3) |
| Counting a sample point on top of a prop as open floor | Pocket check false positives: a prop's top has its own navmesh island, which no path reaches | Project from 10 cm above the floor in a ±20 cm box: the floor's navmesh, never a top 45 cm up (`05` C4) |
| Props shorter than the agent's step height (found in S2 code review) | They don't block: Recast treats them as steps, and their tops can count as floor. A centre-pivot shape on the floor stands only 50 cm × scale tall | Keep the smallest prop above `AgentMaxStepHeight` (35 cm by default): scale ≥ 0.9 here (`05` C3) |
| Taking wall-clock timings from a `-nullrhi` run (found in S2) | Game time runs about 2.5 times faster than the wall clock: S2's seed-1 run generated its last cell after 8.2 s without the GPU and 20.1 s with it, for the same path in game time | Without rendering a frame takes well under 0.5 ms, and the world clamps every frame's game time to at least `MinUndilatedFrameTime` = 0.5 ms (`BaseGame.ini:198`, `AWorldSettings::FixupDeltaSeconds`, `WorldSettings.cpp:334–343`, applied every world tick at `LevelTick.cpp:1599`). Use `-nullrhi` for functional checks only; measure on the GPU (`09` S2) |
| A prop layout whose gaps are only a little wider than the agent | A rare enclosed point: S2's first run had one, in one cell of one seed | Keep the narrowest gap above the agent's width plus the voxel rounding on both sides (`05` C3) |
| Passing a command's arguments on with `%2 … %9` (found in S3 code review) | Only eight arguments arrive; the rest are dropped without an error, so a benchmark step ran with the wrong options | Collect them with `shift` in a loop (`Tools\s3_checks.bat`, `:step`; tested with ten arguments, one of them quoted) |
| An unquoted comma in a batch argument (found in the final job) | `--configs B0,NR` arrived as two arguments: `unrecognized arguments: NR` | cmd splits arguments at commas as well as spaces; quote the list (`"B0,NR"`) |
| `exit /b %ERRORLEVEL%` inside an `if ( … )` block (found in S2 code review) | The script returns the code from **before** the command in the block ran (reproduced: 0 instead of 3), so a failed uncooked run looked fine | cmd expands `%VAR%` when it reads the whole block. Put the command and its `exit /b` on their own lines (`goto` a label), or use delayed expansion `!ERRORLEVEL!` (`05` C6) |

## Tracking readiness (S3)

| Mistake | How to spot it | How to prevent it |
|---|---|---|
| Including `NavMesh/RecastNavMeshGenerator.h` with only a `NavigationSystem` dependency (found in S3) | `fatal error C1083: Cannot open include file: 'Recast/Recast.h'` | Add the `Navmesh` module: `NavigationSystem` depends on it privately, so its headers don't reach dependents (`05` D3b) |
| Making an already populated ISM navigation-relevant and expecting a rebuild (found in the final batch) | NR's cells became "ready" quickly while 0 of 16 had props that blocked the navmesh | An ISM skips the dirty area when it joins the navigation octree (it expects per-instance updates); dirty its bounds yourself with `AddDirtyArea` (`06` §30) |
| Checking the content's own bounds for dirty tiles | A cell reads ready while a tile next to it, dirtied by one of its edge props, is still building | Grow the bounds by the navmesh border first, as the engine grows each dirty area; take the larger of its two growths on every side, since `GrowBoundingBox` and the engine's `GrowDirtyBounds` swap low and high (`05` D3) |
| Broadcasting state changes from inside the per-frame loop | A listener that destroys or spawns a cell shifts the arrays mid-loop | Queue changes; broadcast after the loop, by index and by copy; empty slots instead of removing during the broadcast (`04` §24) |
| Letting the director and NavReady disagree about "done" | The director finishes on the frame navigation goes idle, before NavReady's tick that frame: the last cell still reads `Committed` | Wait for both: idle navigation and every cell `AIReady` (`05` D5) |
| Starting the measurement while the arena's first navmesh build is still running | The first cells' time to navigable includes tiles they never touched | Warm-up waits for a complete path and 2 idle navigation frames (`05` D5) |
| Validating the probe with several cells building at once | The engine's task count returns to 0 only when **all** of them are done, so it can't confirm one cell's moment | Validate with one cell (`-NRCells=1`, `check_probe.py`) |

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
