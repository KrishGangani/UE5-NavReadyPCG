# 05 · Line By Line Code Explanation: NavReadyPCG

**Rule:** every file that exists in `NavReadyPCG\` and `Tools\` is explained here, line by line, in the same step it is written. Sections follow the build order in [03_Implementation_Roadmap.md](03_Implementation_Roadmap.md).

| Part | Step | Files |
|---|---|---|
| A | Phase 4, F1–F3 (foundation) | `.uproject`, two `Target.cs`, two `Build.cs`, two module `.cpp`, `.uplugin`, three config `.ini`, `Tools\*.bat`, `Tools\cpu_affinity.py` |
| B | S1 (world spike) | `NRGraphFactory`, `NRCellActor`, `NRBenchmarkSubsystem` (S1 version, replaced in S2), `NavReadyPCG.Build.cs`, config, `Tools\run_packaged.bat`, `s1_checks.bat` (both removed in S2) |
| C | S2 (content and the determinism hash) | `NavReadyContentHash` and its tests (plugin), `NRGraphFactory` (extended), `NRBenchmarkSubsystem` (S2 version), `NavReadyPCG.Build.cs`, `Tools\run_game.bat`, `run_tests.bat`, `s2_checks.bat`, `compare_hashes.py` |
| D | S3 (readiness observer and harness) | `NavReadyCell`, `NavReadyCellComponent`, `NavReadySubsystem` and their tests (plugin), `NRCellActor` and `NRBenchmarkSubsystem` (S3 changes), `Tools\csv_stats.py`, `run_bench.py`, `check_probe.py`, `s3_checks.bat`, `cpu_affinity.py` |
| E | S5, final (owner's scope cut) | the NavReady commit, the hash at settle, configuration `NR`, `Tools\s5_checks.bat` |

---

## Part A · Foundation (Phase 4)

### A1. `NavReadyPCG.uproject`: the project descriptor

```json
{
	"FileVersion": 3,
	"EngineAssociation": "5.8",
	"Category": "",
	"Description": "P08 NavReadyPCG: runtime generation that's ready to play (UE 5.8.4 portfolio project).",
	"Modules": [ { "Name": "NavReadyPCG", "Type": "Runtime", "LoadingPhase": "Default" } ],
	"Plugins": [ { "Name": "NavReadyPCGCore", "Enabled": true }, { "Name": "PCG", "Enabled": true },
	             { "Name": "AndroidFileServer", "Enabled": false } ]
}
```

| Line | What it does, and why |
|---|---|
| `"FileVersion": 3` | The descriptor schema version. 3 is what Epic's 5.8 `TP_Blank` template uses. A wrong value makes the editor refuse or "upgrade" the file |
| `"EngineAssociation": "5.8"` | Which installed engine opens the project. A launcher engine is identified by its version string. Blank (as in the raw template) means double-clicking the file asks which engine to use |
| `"Category"`, `"Description"` | Shown in the project browser. Cosmetic |
| `"Modules"` | Every C++ module the **project** owns. UnrealBuildTool (UBT) compiles exactly these, plus the modules of enabled plugins |
| `"Name": "NavReadyPCG"` | Must match the folder `Source\NavReadyPCG\` and the class in `NavReadyPCG.Build.cs`. Module names must be unique in a project, because UBT keys them in one case-insensitive dictionary ([research/04](Research/04_local_engine_verification.md) §0). That is why the plugin is `NavReadyPCGCore`, not `NavReadyPCG` |
| `"Type": "Runtime"` | Built into game **and** editor targets. *Alternative:* `Editor` (editor only), which would remove the module from packaged builds |
| `"LoadingPhase": "Default"` | Loads at the normal point, after engine start-up and before the first world. Early enough for the module's classes (later: world subsystems, actors) to exist before the map loads (`06` §4) |
| `"Plugins"` → `NavReadyPCGCore` | Enables the project's own plugin. It is `EnabledByDefault` anyway; listing it makes the dependency visible in one place and keeps it on if that default ever changes |
| `"Plugins"` → `PCG` | Enables Epic's PCG plugin. It is `EnabledByDefault` in 5.8 (`Plugins/PCG/PCG.uplugin:13`), so this line restates a default for the same reason: the project **needs** PCG, and a future engine or a copied project that turns it off would otherwise fail in a confusing way. It is also required once the game module depends on the `PCG` module (from S1): UBT warns when a project module depends on a plugin module that the `.uproject` doesn't list (`Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:1687`). *Not listed:* `NavigationSystem`, because it is an engine **module**, not a plugin; it is always present |
| `"Plugins"` → `AndroidFileServer`, `"Enabled": false` | **Switches off** an engine plugin that is `EnabledByDefault` (`Plugins/Runtime/AndroidFileServer/AndroidFileServer.uplugin:15`). Its editor module (`AndroidFileServerEditor`) writes an `[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]` section with a **random `SecurityToken`** into `Config\DefaultEngine.ini` on the first editor run (it did, in F-3 of run 1). A connection token must not reach a public repository, and the plugin (Epic's file server for Android devices) has no use in a Windows-only project. Added after run 2, on the portfolio-wide rule found while auditing UE5-SpawnPacer |

### A2. `Source\NavReadyPCG.Target.cs` and `NavReadyPCGEditor.Target.cs`: build targets

```csharp
// P08 NavReadyPCG - game target (standalone/packaged builds).

using UnrealBuildTool;

public class NavReadyPCGTarget : TargetRules
{
	public NavReadyPCGTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("NavReadyPCG");
	}
}
```

| Line | What it does, and why |
|---|---|
| `using UnrealBuildTool;` | Target files are **C#**, compiled by UBT itself (`04` §5). This brings `TargetRules`, `TargetType` and so on into scope |
| `public class NavReadyPCGTarget : TargetRules` | UBT finds targets by class name, `<Name>Target`. This class defines the target `NavReadyPCG`, which is why `build.bat NavReadyPCG …` works. The editor file defines `NavReadyPCGEditorTarget` → target `NavReadyPCGEditor` |
| `(TargetInfo Target) : base(Target)` | UBT passes in the platform, configuration and architecture being built; the base constructor stores them |
| `Type = TargetType.Game` | A standalone game executable with no editor code: the target that gets packaged. The editor file uses `TargetType.Editor`, which builds DLLs that `UnrealEditor.exe` loads |
| `DefaultBuildSettings = BuildSettingsVersion.V7` | Opts into the newest default compiler and build settings for this engine. V7 is what Epic's 5.8 template uses; older values keep older defaults for backward compatibility |
| `IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8` | Uses 5.8's engine header include order (Epic trims transitive includes between versions). Pinned, so an engine upgrade can't silently change what our code includes |
| `ExtraModuleNames.Add("NavReadyPCG")` | Which of the project's modules this target builds. Plugin modules come in through the plugin system |

**Why two target files?** The editor and a packaged game are different executables with different module sets. UBT needs one target rule per executable type. No `Server`/`Client` targets: v1 is single-player (`02` §1).

### A3. `Source\NavReadyPCG\NavReadyPCG.Build.cs`: the game module's build rules

```csharp
public class NavReadyPCG : ModuleRules
{
	public NavReadyPCG(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		// Private: no public header of this module includes a plugin header yet (Phase 4 has no classes).
		PrivateDependencyModuleNames.AddRange(new string[] { "NavReadyPCGCore" });
	}
}
```

| Line | What it does, and why |
|---|---|
| `class NavReadyPCG : ModuleRules` | The class name **must equal the module name**; UBT maps each `*.Build.cs` to its module |
| `ReadOnlyTargetRules Target` | A module can read the target's settings (platform, configuration) but not change them, so module rules can't fight each other |
| `PCHUsage = UseExplicitOrSharedPCHs` | Use a **shared precompiled header** the engine provides (the build log shows `SharedPCH.UnrealEd.Project…` for the editor and `SharedPCH.Engine.Project…` for the game), unless the module names its own. Precompiled headers make each `.cpp` compile faster. *Rejected:* `NoPCHs` (slow), a private PCH (pointless for a small module) |
| `PublicDependencyModuleNames …` | Modules whose headers **our public headers** will include: `Core` (containers, strings, logging), `CoreUObject` (UObject, reflection, GC), `Engine` (actors, components, worlds). Anything that depends on our module inherits these include paths |
| `PrivateDependencyModuleNames … "NavReadyPCGCore"` | Our plugin, private for now: only `.cpp` files will use it. **Rule of thumb: private unless a public header needs it** (`06` §2). When `S1`+ adds a public header that includes a plugin header, this moves to public in the same step. `PCG` and `NavigationSystem` are added here when the game module's code first uses them (S1/S2), not before |
| *Not included:* `InputCore`, `EnhancedInput` | The template adds them, but this project reads no player input in code |

### A4. `Source\NavReadyPCG\Private\NavReadyPCGModule.cpp`

```cpp
// P08 NavReadyPCG - primary game module registration.

#include "Modules/ModuleManager.h"

// The game module needs no startup logic, so the engine's default implementation is enough.
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, NavReadyPCG, "NavReadyPCG");
```

| Line | What it does, and why |
|---|---|
| `#include "Modules/ModuleManager.h"` | Declares `IMPLEMENT_PRIMARY_GAME_MODULE` and `FDefaultGameModuleImpl` (`Core/Public/Modules/ModuleManager.h`). Include the specific header, not a giant one ("include what you use") |
| `IMPLEMENT_PRIMARY_GAME_MODULE(…)` | Generates the module's entry point (`06` §1). In a monolithic (packaged) build it also defines project-wide globals such as the internal project name. **Primary** means "the project's main game module"; a project needs exactly one. *Without it:* the module never registers, and a packaged game has no project name to start from |
| `FDefaultGameModuleImpl` | The engine's module class with empty `StartupModule`/`ShutdownModule`. Writing our own empty class would be an extra class with no reason |
| `NavReadyPCG` | The module name; must match the `.uproject` and `Build.cs` |
| `"NavReadyPCG"` | The old "game name" argument. In 5.8 the macro ignores it (the monolithic overloads call it `DEPRECATED_GameName`, `Core/Public/Modules/ModuleManager.h:1100, 1114`), but it still takes three arguments, so we pass the project name, as the template does |

**Why there is no `NavReadyPCG.h`:** see `04` §4.

### A5. `Plugins\NavReadyPCGCore\NavReadyPCGCore.uplugin`: the plugin descriptor

| Field | Value | Why |
|---|---|---|
| `FileVersion`, `Version`, `VersionName` | 3, 1, "1.0" | The same schema as Epic's plugins in 5.8 |
| `FriendlyName`, `Description`, `Category` | "NavReady PCG Core", one sentence, "AI" | Shown in the editor's Plugins window. "AI" because the plugin's output is "AI may use this cell now" |
| `EnabledByDefault` | `true` | A project that copies the plugin gets it working without editing its `.uproject` |
| `CanContainContent` | `false` | Code only; saying so avoids an empty content mount point |
| `IsBetaVersion`, `IsExperimentalVersion` | `false`, `false` | No warning badge. That is a statement about API stability inside this project, **not** a claim of production readiness |
| `Installed` | `false` | Part of the project, not installed into the engine |
| `Modules[0]` | `NavReadyPCGCore`, `Runtime`, `Default` | One runtime module at the default loading phase |
| `"Plugins": [ { "Name": "PCG", "Enabled": true } ]` | Declares that this plugin **depends on the PCG plugin**. Required because the module links against the `PCG` module: without this entry UBT warns "Plugin '…' does not list plugin 'PCG' as a dependency, but module '…' depends on module 'PCG'" (`Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:1578`), and the brief requires 0 warnings. It also means a project that enables NavReadyPCGCore gets PCG enabled automatically |

### A6. `NavReadyPCGCore.Build.cs` and `Private\NavReadyPCGCoreModule.cpp`

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
// The two engine systems this plugin coordinates. Declared in Phase 4, before any code uses them, to prove early that
// a launcher (binary) engine links them into both the editor and the packaged game (03_Implementation_Roadmap.md F2).
PrivateDependencyModuleNames.AddRange(new string[] { "PCG", "NavigationSystem" });
```

| Line | What it does, and why |
|---|---|
| Public `Core`, `CoreUObject`, `Engine` | The plugin's public headers will expose engine types (`UActorComponent`, `UTickableWorldSubsystem`), so every module that includes them needs these include paths |
| Private `PCG`, `NavigationSystem` | The two systems the plugin coordinates. **Private** because its public headers will only forward-declare `UPCGComponent` and never mention navigation types (`02` §3). **Declared before any code uses them**, which normally breaks the "no dependency without a user" rule: the reason is the brief's instruction to check early whether the project needs anything a launcher engine can't provide. F-1/F-2 prove that both link into the editor and the game; F-5/F-6 prove the packaged game has them (their console variables exist) |
| `IMPLEMENT_MODULE(FDefaultModuleImpl, NavReadyPCGCore);` | The non-primary version of A4: an ordinary module entry point with the engine's empty default implementation |

The build-log line `[Adaptive Build] Excluded from NavReadyPCGCore unity file: NavReadyPCGCoreModule.cpp` is normal: UBT compiles files that git reports as changed on their own instead of inside the "unity" batch file, to speed up iteration.

### A7. `Config\DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`

**`DefaultEngine.ini`**

| Section / key | Why |
|---|---|
| `GameMapsSettings: GameDefaultMap / EditorStartupMap = /Engine/Maps/Templates/Template_Default` | A shipped engine map with a floor and lights, so Phase 4 needs no authored map and the project stays buildable from text. In S1 the director builds the arena at runtime; the fallback is a scripted map (`02` §12) |
| `[/Script/Engine.Engine] bSmoothFrameRate=False, bUseFixedFrameRate=False` | Both are engine defaults, **restated** so nobody can turn on smoothing or a frame cap later without it showing in a diff. Either would make frame time stop reflecting real work |
| `[SystemSettings] r.VSync=0` | Also a default, restated for the same reason: VSync makes Present wait for the monitor |
| `RendererSettings`, `WindowsTargetSettings` | Copied from Epic's 5.8 template: DX12, virtual shadow maps, no static lighting. `-D3D12TargetedShaderFormats=PCD3D_SM5` removes the SM5 entry that `BaseEngine.ini:3412` adds, and `+…=PCD3D_SM6` adds SM6. The SM5 shader archives in the package log come from `BaseEngine.ini:3413`'s D3D11 entry, which the template leaves alone |
| *Comment at the top* | Navigation settings (`RuntimeGeneration=Dynamic`) are deliberately **not** here yet: they arrive in S1 together with the check that proves the setting takes effect |

**Engine-written block (removed):** on the first editor run (F-3 of run 1), the engine's `AndroidFileServerEditor` plugin appended an `[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]` section with a random `SecurityToken`. It configures Epic's Android device file server and does nothing on Windows, but a connection token must not be published. The block was deleted, the plugin is disabled in the `.uproject` (A1) so it can't be written again, and run 3 checks that the file stays clean after an editor run (`09`).

**`DefaultGame.ini`** sets `ProjectName` and `Description` (`[/Script/EngineSettings.GeneralProjectSettings]`): shown in window titles and crash reports.

**`DefaultInput.ini`** (added after the first check run):

```ini
[/Script/Engine.InputSettings]
DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput
DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent
```

| Line | Why |
|---|---|
| `DefaultPlayerInputClass=…EnhancedPlayerInput` | The engine's own default is the legacy `UPlayerInput` (`Runtime/Engine/Private/UserInterface/InputSettings.cpp:52`). The Enhanced Input plugin, enabled by default, casts the player's input object to `UEnhancedPlayerInput` (`Plugins/EnhancedInput/Source/EnhancedInput/Private/EnhancedInputSubsystems.cpp:79`) and logs `LogEnhancedInput: Warning: … does not have a valid PlayerInput object` when the cast fails (`:118`). The first check run showed exactly that warning in every game run (`09` F-3, F-5, F-6). These are the values Epic's 5.8 template ships (`Templates/TP_Blank/Config/DefaultInput.ini:79–80`) |
| `DefaultInputComponentClass=…EnhancedInputComponent` | The matching input component class, from the same template lines. Without it, actors would get the legacy component, which Enhanced Input bindings can't use |
| *Not copied:* the template's ~80 axis and key lines | They only restate engine defaults for gamepads and VR controllers; this project reads no input |

### A8. `Tools\`: build, package, smoke and check scripts

Every Unreal job in these scripts is run **through the machine-wide queue** (`tools\ue_queue.py run …`), never directly, so builds and benchmarks of the ten portfolio projects never overlap.

| Script | Key lines | Why |
|---|---|---|
| `build.bat [Target] [Config]` | `call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %TARGET% Win64 %CONFIG% -Project="%PROJECT%" -WaitMutex -NoHotReloadFromIDE` | A reproducible command-line build. `%~dp0` is the script's own folder, so it works from any working directory; paths are quoted because the folder name contains a space. `-WaitMutex` waits for another UBT instead of failing. The default target is the editor, the default configuration Development |
| `package.bat [Config]` | `RunUAT.bat BuildCookRun -project=… -noP4 -utf8output -unattended -platform=Win64 -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory=…` | Builds, cooks, stages, paks and archives a standalone game into `..\Build` (git-ignored). Development is the default because the launcher engine can't build Test (UE5-UpdateBudget, Phase 4) |
| `smoke_game.bat` | `UnrealEditor-Cmd.exe <uproject> -game -nullrhi -nosound -unattended -nosplash -traceautostart=0 -ExecCmds="pcg.FrameTime,pcg.RuntimeGeneration.NumGeneratingComponents,ai.navigation.MaxDirtyAreasCountWhileSuspended,quit" -log -abslog=…` | Runs the **uncooked** game with no window or GPU, loads the default map, then runs four console commands. Typing a console variable's name without a value prints its value to the log. A console variable exists only once the module that defines it has started, so seeing `pcg.*` and `ai.navigation.*` values proves PCG and the navigation system are loaded. `quit` ends the run. `-traceautostart=0` stops an open Unreal Insights from tracing the run. *Removed after the first run:* `-ExitAfterSeconds=0`, copied from UE5-UpdateBudget's script, because nothing in 5.8.4 reads it (no match in `Engine/Source` or `Engine/Plugins/Runtime`, grep 2026-11-21) |
| `smoke_packaged.bat [nullrhi\|gpu]` | The same `-ExecCmds` against `..\Build\Windows\NavReadyPCG.exe`; `gpu` mode adds `-windowed -ResX=960 -ResY=540` | Proves the **packaged** build contains both systems, and (GPU mode) that it runs on the RTX GPU rather than the integrated one (the log names the chosen adapter). `%2 … %9` passes extra arguments through, since batch has no "all but the first argument" variable |
| `phase4_checks.bat` | `call :step F-1 … \|\| exit /b` six times; `:step` prints start/end time stamps and the exit code | All six foundation checks in **one** queue job, so the project waits in line once instead of six times. It stops at the first failure and returns that exit code, so the queue records the job as failed |

### A9. `Tools\cpu_affinity.py`: pinning timed runs to the performance cores

Not used in Phase 4 (nothing is timed yet), but written now because the measurement protocol depends on it (`01` §7). `performance_core_mask()`, `pin_self()`, `applied_mask()` and the `KERNEL32` set-up are copied from UE5-UpdateBudget's `run_bench.py` so this repository stays standalone; `run_game()` is the launch step of the same protocol.

| Part | What it does, and why |
|---|---|
| `KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)` + `restype`/`argtypes` lines | Loads the Windows kernel API through `ctypes` and declares the real C types of the functions used. Without the declarations, `ctypes` assumes 32-bit `int` arguments and results, which would truncate the 64-bit process handle and affinity masks. Guarded by `sys.platform == "win32"` so the module still imports elsewhere |
| `performance_core_mask()` | Asks Windows for one record per physical core (`GetLogicalProcessorInformationEx` with `RelationProcessorCore` = 0). Each record is an 8-byte header (`Relationship`, `Size`) followed by a `PROCESSOR_RELATIONSHIP`: `Flags` (byte 8), **`EfficiencyClass` (byte 9)**, 20 reserved bytes, `GroupCount` (bytes 30–31), then `GroupMask[0]`: an 8-byte **affinity `Mask`** (bytes 32–39) and a 2-byte `Group` (bytes 40–41). `size >= 48` checks the record is long enough to hold that first group mask. The masks are combined per efficiency class; P-cores have the higher class, so the highest class's mask is returned. `None` if there is only one class (a non-hybrid CPU). Layout: Microsoft's `PROCESSOR_RELATIONSHIP` documentation, https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-processor_relationship |
| Two calls to `GetLogicalProcessorInformationEx` | The first, with no buffer, only reports the size needed; the second fills a buffer of that size. A standard Win32 pattern |
| `pin_self(mask)` | `SetProcessAffinityMask(GetCurrentProcess(), mask)`: restricts **the batch script itself**. A process inherits its parent's affinity when it is created, so every game the script starts afterwards runs on those CPUs from its first instruction, engine start-up included. *Rejected:* pinning each game after it starts (`OpenProcess` + `SetProcessAffinityMask` on its PID): it works, but leaves a short start-up window unpinned and needs one more Win32 call per run. The batch script calls `pin_self` once, before anything is recorded or started, and refuses to measure if it fails |
| `applied_mask()` | Reads the mask back with `GetProcessAffinityMask`, so the environment record shows what Windows actually applied, not what was requested. If the process mask equals the system mask, it says "all logical processors" |
| `run_game(command, timeout)` | `subprocess.Popen` on the **real** game executable (not the bootstrap launcher, which would be a different process with its own exit code), `wait(timeout)`, and in `finally` kill the whole process tree with `taskkill /F /T` if it is still running: a timeout, Ctrl+C or any error can never leave a game running into the next run. Returns the real exit code, or `None` on timeout |
| *Rejected:* `cmd /c start "" /wait /affinity …` | It always returns exit code 0, whatever the game returned (found by UE5-TestTrust), so failed runs would look fine |
| `if __name__ == "__main__"` | `python -I cpu_affinity.py` prints the mask. On 2026-11-21 it printed `0xFFFF` (logical CPUs 0–15: the 8 P-cores with hyper-threading) |

**Self-test (2026-11-21):** the script's own mask was `0xFFFFFF` (all 24 logical CPUs) before `pin_self` and `0xFFFF` after; a PowerShell child started with `run_game` reported its `ProcessorAffinity` as 65535 (= `0xFFFF`), so it inherited the mask; `run_game(["cmd", "/c", "exit 3"], 10)` returned 3; a 6-second `ping` with a 1-second timeout returned `None` and was killed.

**Phase 4 results** are recorded in [09_Testing_And_Benchmarks.md](09_Testing_And_Benchmarks.md) (F-1 to F-6).

---

## Part B · Step S1: the world spike

**What S1 had to prove (`03` S1, `01b #8`):** that a world built entirely at runtime, in a **packaged** build, gets a navmesh and runs PCG's runtime-generation scheduler. Result: it does (`09` S1). Everything below is the code that proves it, and most of it stays as the base of the benchmark director.

**Files:** `Private\NRGraphFactory.h/.cpp` (B1), `Public\NRCellActor.h` + `Private\NRCellActor.cpp` (B2), `Public\NRBenchmarkSubsystem.h` + `Private\NRBenchmarkSubsystem.cpp` (B3), `NavReadyPCG.Build.cs` (B4), config (B5), `Tools\` (B6).

### B1. `NRGraphFactory.h` / `.cpp`: the PCG graph, built in code

```cpp
namespace NRGraphFactory
{
	struct FRockFieldParams { FVector VoxelSize = FVector(300.0, 300.0, 1000.0); TSoftObjectPtr<UStaticMesh> Mesh; bool bNavigationVisible = true; };
	UPCGGraph* BuildRockFieldGraph(UObject* Outer, const FRockFieldParams& Params);
}
```

| Line | What it does, and why |
|---|---|
| `namespace NRGraphFactory` + free functions | No state, no lifetime: a namespace of functions is enough (`02` §4.2). *Rejected:* a `UObject` "factory" class, which would need its own lifetime for nothing |
| `struct FRockFieldParams` | Every graph setting in one plain struct, so the director can vary them per run (S2 adds the transform parameters and a second mesh, `05` C3) |
| `VoxelSize = (300, 300, 1000)` | One point every 3 m in X and Y. Z is larger than the cell box is tall, so there is exactly one layer of points |
| `TSoftObjectPtr<UStaticMesh> Mesh` | The spawner's mesh entry stores a soft reference (`FPCGMeshSelectorWeightedEntry(TSoftObjectPtr<UStaticMesh>, int)`); the director passes the already-loaded cube, so nothing is loaded asynchronously during generation |
| `bNavigationVisible` | Becomes the spawner descriptor's *Can Ever Affect Navigation*: `true` in S1 and the baselines, `false` for NavReady (`02` §9) |
| `NewObject<UPCGGraph>(Outer, NAME_None, RF_Transient)` | A graph object that lives only in memory. `Outer` (the director) keeps it alive through the director's `UPROPERTY` reference; `RF_Transient` says it is never saved |
| `AddNodeOfType<UPCGVolumeSamplerSettings>(Sampler)` | Adds a node and returns its settings object through the reference parameter. **Why a Volume Sampler, not Create Points Grid:** `UPCGCreatePointsGridSettings` is declared `UCLASS(BlueprintType)` with no `MinimalAPI`/`PCG_API` (`Plugins/PCG/Source/PCG/Public/Elements/PCGCreatePointsGrid.h:27`), so its `StaticClass()` is not exported and can't be linked from our module. `UPCGVolumeSamplerSettings` is `MinimalAPI` (`PCGVolumeSampler.h:65`) and samples a regular voxel grid inside its input volume, which gives the same grid of points |
| `Sampler->VoxelSize = Params.VoxelSize` | A public `UPROPERTY` (`PCGVolumeSampler.h:74`). Writing a data member needs no export, only the class layout from the header |
| `AddNodeOfType<UPCGStaticMeshSpawnerSettings>(Spawner)` | The spawner (`MinimalAPI`, `PCGStaticMeshSpawner.h:26`). Its constructor already creates a `UPCGMeshSelectorWeighted` (`PCGStaticMeshSpawner.cpp`, constructor) |
| `CastChecked<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters)` | `CastChecked` asserts instead of returning null: if the engine ever changes the default selector, the build fails loudly here rather than generating nothing |
| `MeshEntries.Emplace_GetRef(Params.Mesh, 1)` | Adds one weighted entry (weight 1, the only entry) and returns a reference to it, so its descriptor can be edited in place |
| `Entry.Descriptor.BodyInstance.SetCollisionProfileName(BlockAll)` | PCG's descriptor defaults to `NoCollision` (`PCGISMDescriptor.cpp:11–19`). `BlockAll` blocks Pawn, which is what makes the props navigation-relevant (`01b #2`) |
| `Entry.Descriptor.bCanEverAffectNavigation = …` | The per-mesh-entry flag NavReady relies on (`ISMComponentDescriptor.h:236–237`) |
| `AddEdge(GetInputNode(), PCGPinConstants::DefaultInputLabel, SamplerNode, VolumeLabel)` | The graph input node's output pin is `"In"` (`PCGPinConstants::DefaultInputLabel`, `PCGInputOutputSettings.cpp:27–36`). For a PCG component on an actor it carries the actor's data; our cell actor's `UBoxComponent` becomes analytic collision-shape data (`Data/Registry/PCGGetDataFunctionRegistry.cpp:321–327`), which the sampler fills with points. **Found in S1:** the similarly named `PCGInputOutputConstants::DefaultInputLabel` is `"Input"` and doesn't exist on the input node (`LogPCG: Error: From node DefaultInputNode does not have the Input label`) |
| `AddEdge(SamplerNode, DefaultOutputLabel, SpawnerNode, DefaultInputLabel)` | Points → spawner. Nothing is wired to the graph output node: spawners act through side effects, and the compiler only culls nodes whose **inputs** are unwired (`Graph/PCGGraphCompiler.cpp:790–815`) |
| `IsInputPinConnected(…)` checks | **Neither edge function reports success:** `AddEdge` always returns the target node, and `AddLabeledEdge` returns whether the target pin *dropped other edges* (`PCGGraph.h:504`), so it is `false` on success too. S1's second run reported a correct graph as "could not wire" because of that. The target pins themselves are the reliable check (`PCGNode.h:106`) |
| `return nullptr` + `UE_LOG(Error)` | A graph that isn't wired generates nothing; the caller refuses to spawn a cell with it |

### B2. `NRCellActor.h` / `.cpp`: one cell

| Line | What it does, and why |
|---|---|
| `UCLASS() class ANRCellActor : public AActor` | An actor, because PCG spawns its instance components *onto an actor* (the component's owner, by default). Not exported (no `_API`): only this module uses it |
| `UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> Bounds` | The cell volume. `UPROPERTY` makes the reference visible to GC and reflection; `TObjectPtr` is the 5.x member pointer type for UObjects (`06` §7) |
| `UPROPERTY(VisibleAnywhere) TObjectPtr<UPCGComponent> PCG` | The generator. `UPCGComponent` is only forward-declared in the header, so the header doesn't need PCG's include path (`B4`) |
| Constructor: `PrimaryActorTick.bCanEverTick = false` | Cells never tick; PCG and the director do the work |
| `CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"))` | Creates the component as part of the class's default object, so every instance gets one; the name must be unique within the actor |
| `SetCollisionProfileName(NoCollision)`, `SetGenerateOverlapEvents(false)` | The box is a volume for PCG only; physics never sees it |
| `SetCanEverAffectNavigation(false)` | Shape components can affect navigation by default (`ShapeComponent.cpp:61`). A cell volume must not change the navmesh |
| `PCG->GenerationTrigger = GenerateAtRuntime` | Hands the component to PCG's runtime-generation scheduler (`PCGComponent.h:77–82`) |
| `bool Configure(Graph, Seed, HalfExtent, GenerationRadius)` | Everything the component needs before its `BeginPlay`, where it registers with the PCG subsystem (`PCGComponent.cpp:1743–1767`). That is why the director spawns cells deferred and calls this before `FinishSpawning` |
| `Bounds->SetBoxExtent(HalfExtent, false)` | Sets the cell size; `false` = don't update overlaps (there are none) |
| `PCG->Seed = Seed` | The component seed (`PCGComponent.h:314`). S2 gives each cell its own |
| `SetRadiiMember(…)` (reflection) | Non-partitioned runtime components generate when a generation source comes within `RadiusUnbounded` of their bounds (`PCGRuntimeGenScheduler.cpp:937–983`). The default is `(1u << 31) × 2` cm (`PCGCommon.h:551, 626`): effectively "generate now". `RadiusUnbounded` and `CleanupRadiusScalar` are **protected** C++ members (`PCGCommon.h:623–660`) but `BlueprintReadWrite` UPROPERTYs, so the code writes them through reflection, the same path a Blueprint uses: find `UPCGComponent::GenerationRadii` as an `FStructProperty`, find the member inside its struct, check it is an `FPerQualityLevelFloat`, then write through `ContainerPtrToValuePtr`. The type is compared **by name** because `FPerQualityLevelFloat` isn't exported from the Engine module, so its `StaticStruct()` can't be linked (`PerQualityLevelProperties.h:268–269`) |
| `CleanupRadiusScalar = 100` | Cleanup happens beyond radius × scalar: 3.5 km here, so no cell is cleaned up during a run (`01` §6.1) |
| `bOverrideGenerationRadii = bRadiiSet` | The override is switched on only if both writes succeeded; otherwise `Configure` returns `false` and the director destroys the cell instead of letting it generate at once (code-review finding) |
| `GenerationRadii.ComputeHash()` | "Call this anytime the values are changed" (`PCGCommon.h:615–616`) |
| `SetSchedulingPolicyClass(UPCGSchedulingPolicyDistanceAndDirection::StaticClass())` | The constructor only sets the policy **class** (`PCGComponent.cpp:118`); the scheduler `ensure`s that a policy **object** exists (`PCGRuntimeGenScheduler.cpp:937`). This call creates it (`PCGComponent.cpp:1372–1376`) |
| `PCG->SetGraphLocal(Graph)` | Assigns the graph without the network-multicast wrapper `SetGraph` has. `PCGGraph.h` is included so the compiler knows `UPCGGraph` derives from `UPCGGraphInterface` (with only a forward declaration the conversion fails: `error C2664`) |
| `GetCellBounds()` | The box's world bounds (`Bounds->Bounds`, updated when the component registers) |
| `GetGeneratedComponents(Out)` | Every `UInstancedStaticMeshComponent` on the actor, minus those without PCG's `"PCG Generated Component"` tag (`PCGHelpers.h:35`). One helper, used by the spike now and by the hash (S2) and the commit (S5) later (simplify finding: the loop was written twice) |

### B3. `NRBenchmarkSubsystem.h` / `.cpp`: the director (S1 version)

> **Replaced in S2** by the run director (C4); the spike checks below are gone from the code. They stay documented because `09` S1's results came from this version (commit `474d40f`).

**Class and lifetime**

| Line | What it does, and why |
|---|---|
| `UCLASS() class UNRBenchmarkSubsystem : public UTickableWorldSubsystem` | One instance per world, created and destroyed with it, ticking every frame (`06` §8). *Rejected:* a director actor (nothing would place it: there is no authored map) |
| `DoesSupportWorldType` → Game, PIE | World subsystems exist in editor worlds too; the director must never build an arena there |
| `OnWorldBeginPlay(UWorld&)` | Runs once, before actors' `BeginPlay` (UE5-UpdateBudget `01b` §3). Reads the command line, then builds the arena and the test cell |
| `FParse::Param(CommandLine, TEXT("NRSpike"))` | `true` if `-NRSpike` is present. Without it the world stays the plain template map (Phase 4's smoke runs behave exactly as before) |
| `FParse::Value(CommandLine, TEXT("NRTimeout="), TimeoutSeconds)` | Optional; the default 60 s is used when absent |
| `if (!BuildArena(InWorld) \|\| !SpawnTestCell(InWorld)) { FinishChecks(0.0); return; }` | On a set-up failure the summary line is still printed and every check reported as failed (code-review finding: an early return used to skip the summary) |
| `GetStatId()` with `RETURN_QUICK_DECLARE_CYCLE_STAT` | Required by tickable objects; names the tick in `stat` output |

**`BuildArena`**

| Line | What it does, and why |
|---|---|
| `LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))` | A synchronous load of the engine cube (cooked because `DefaultGame.ini` lists `/Engine/BasicShapes`, B5). Loading once here means generation never waits on an asynchronous mesh load |
| `NRBench::ArenaCenter = (100000, 0, 0)` | 1 km from the origin, away from the template map's own floor (`SM_Template_Map_Floor`, at the origin) so the arena's navigation bounds contain only our geometry |
| `SpawnParams.SpawnCollisionHandlingOverride = AlwaysSpawn` | Never refuse or nudge a spawn because something overlaps |
| Floor: `SpawnActor<AStaticMeshActor>` → `SetMobility(Movable)` → `SetStaticMesh` → `SetActorScale3D` | A 200 m × 200 m × 10 cm slab, top at z = 0. **Movable** because `SetStaticMesh` refuses to change a Static component once the world has begun play (`StaticMeshComponent.cpp`, `SetStaticMesh`: "Calling SetStaticMesh on '…' but Mobility is Static"). Setting the mesh also re-registers the component with navigation (`FNavigationSystem::UpdateComponentData`, same function) |
| `SpawnActor<ANavMeshBoundsVolume>` + a `UBoxComponent` "ArenaExtent" | A bounds volume spawned at runtime has no brush geometry (the editor builds that), so on its own it reports no area. Navigation takes a volume's area from `GetComponentsBoundingBox(true)` (`NavigationSystem.cpp:4400–4413`), which includes every registered primitive component, so a box with the arena's extent gives it one. The box has no collision and `SetCanEverAffectNavigation(false)` |
| `NewObject<UBoxComponent>(Volume, …)` → `SetupAttachment` → `RegisterComponent` | Creating a component at runtime: construct it with the actor as outer, attach it before registering, then register (which creates its render/physics/navigation state) |
| `NavSys->OnNavigationBoundsUpdated(Volume)` | The volume registered itself during the spawn with an empty box (`NavMeshBoundsVolume.cpp:117–125`); this sends the real box as an `Updated` request (`NavigationSystem.cpp`, `OnNavigationBoundsUpdated`). The engine then **spawns the missing `RecastNavMesh` itself** (`NavigationSystem.cpp:4375–4390`); with `RuntimeGeneration=Dynamic` configured it does so without a warning (`:4661–4668` warns only for `Static`), and builds it |
| `SpawnActor<APCGWorldActor>(APCGWorldActor::StaticClass(), …)` | Runtime generation only ticks when a PCG world actor is registered (`PCGSubsystem.cpp:425–428`), and nothing creates one at runtime (`:431–443`). It registers itself in `BeginPlay` (`PCGWorldActor.cpp:187–191`). `StaticClass()` links because the class is `MinimalAPI` (`PCGWorldActor.h:16`) |

**`SpawnTestCell`**

| Line | What it does, and why |
|---|---|
| `BuildRockFieldGraph(this, GraphParams)`; `if (!Graph) return false;` | The graph first: no cell is spawned for a graph that failed |
| `SpawnActorDeferred<ANRCellActor>(…)` | Constructs the actor and its components but defers `BeginPlay` until `FinishSpawning`, so `Configure` runs first (B2) |
| `if (!Cell->Configure(…)) { Cell->Destroy(); return false; }` | Never leaves a deferred, unfinished actor in the world (code-review finding) |
| `OnPCGGraphStartGeneratingDelegate.AddUObject(this, …)`, `OnPCGGraphGeneratedDelegate.AddUObject(this, …)` | The component's native multicast delegates (`PCGComponent.h:380–383`). `AddUObject` binds weakly: if the director is destroyed first, the call is skipped instead of crashing |
| `FinishSpawning(FTransform(CellLocation))` → `TestCell = Cell` | `BeginPlay` runs here: the PCG component registers with the subsystem and from then on the scheduler considers it |

**Camera and the generation source**

| Line | What it does, and why |
|---|---|
| `SpawnActor<ACameraActor>()`, `UpdateCamera(Elapsed)` | The camera starts 70 m before the cell, 15 m up, pitched −45°, and flies towards it at 5 m/s |
| `PlayerController->SetViewTarget(Camera)`, re-applied each frame | PCG's player generation source takes its position from `PlayerController->GetPlayerViewPoint` (`RuntimeGen/GenSources/PCGGenSourcePlayer.cpp`, `Tick`), which is the view target. So the camera is what drives runtime generation |
| `CameraDistanceToCell()` | 2D distance from the camera to the cell's box (`FBox::ComputeSquaredDistanceToPoint` with the point moved to the box's floor), the same measure the scheduler uses for non-partitioned components with a 2D grid (`PCGRuntimeGenScheduler.cpp:974–980`) |

**The three checks** (each retried every frame until it passes; the run ends when all pass, when generation fails, or at the timeout)

| Check | Code | Pass means |
|---|---|---|
| 1. Navmesh | `CheckArenaPath`: `FindPathSync(FPathFindingQuery(this, *NavData, Start, End))` corner to corner, 5 m inside the edges, 50 cm up | A complete (`IsSuccessful() && !IsPartial()`) path across the whole arena: the runtime navmesh exists and spans it |
| 2. Runtime generation | `HandleCellGenerationStarted` records the camera distance; `HandleCellGenerated` counts instances via `GetGeneratedComponents` | Generation started within the radius but not at the very start (the camera began outside it), and produced instances: the scheduler, not something else, triggered it |
| 3. Carve | `CheckPropCarved`: `ProjectPointToNavigation` under prop 0 and half-way to the next prop, 10 cm above the floor with a ±20 cm vertical extent, after navigation has been idle for 2 frames | No navmesh under the prop, navmesh between props: the props reached navigation **without any coordination** (`01b #2`, measured) |

| Line | What it does, and why |
|---|---|
| `enum class EVerdict : uint8 { Pending, Pass, Fail }` | Three states, not magic `0/1/-1` integers (simplify finding) |
| Retry until pass | A single attempt could read navigation in a transient state: the bounds update and a new component's octree registration are deferred to the next navigation tick (`01b #7`). A pass can't be transient (a complete path; floor navmesh beside a prop and none under it), so retrying removes false failures without risking false passes (code-review findings 2–4) |
| `CarveQueryHeight = 10`, `CarveQueryExtent = (5, 5, 20)` | Only floor-level navmesh directly under the point counts; a walkable patch Recast might leave on top of a 1 m prop is out of range (code-review finding 5) |
| `NRBench::GetNavigation(World, OutNavSys)` | One lookup of the navigation system and its default navigation data, used by both checks (simplify finding) |
| `FinishChecks(Elapsed)` | Turns pending verdicts into failures, logs one summary line in every case, and `Error`-logs `NRSpike: FAILED.` on a failure |
| Exit: `RequestExit(false)` on a pass, `RequestExitWithStatus(true, 1)` on a failure | **Found in S1:** a non-forced exit drops the return code on Windows: it only posts `WM_QUIT` (`Core/Private/Windows/WindowsPlatformMisc.cpp:1517–1521`), and the failed run exited 0. The forced exit flushes the log and calls `TerminateProcess(…, 1)` (`:1493–1515`), so scripts and the queue see the failure. A passing run still shuts down cleanly |
| `ExitDelayFrames = 3` | A few frames between the verdict and the exit, so the log is written |

### B4. `NavReadyPCG.Build.cs`

```csharp
PrivateDependencyModuleNames.AddRange(new string[] { "NavReadyPCGCore", "PCG", "NavigationSystem" });
```

`PCG` (cell actors, the graph, the world actor) and `NavigationSystem` (bounds volume, path queries) are **private**: the module's public headers only forward-declare `UPCGComponent`, `UPCGGraph` and `ACameraActor` (`06` §2). The `.uproject` already lists the `PCG` plugin, so UBT's project-level dependency check is satisfied (`05` A1). The empty `Public\.gitkeep` was removed: the folder now holds real headers.

### B5. Configuration added in S1

| File / key | Why |
|---|---|
| `DefaultEngine.ini` `[/Script/NavigationSystem.RecastNavMesh] RuntimeGeneration=Dynamic` | The engine default is `Static` (`BaseEngine.ini:3042–3043`), which never rebuilds at runtime; generated content needs `Dynamic` (`01b` §3). It also decides that the runtime-spawned navmesh is created without the "No saved navigation data found" warning (`NavigationSystem.cpp:4661–4668`). The S1 spike's complete arena path proves the setting takes effect |
| `DefaultEngine.ini` `[/Script/NavigationSystem.NavigationSystemV1] CrowdManagerClass=` | No Detour crowd manager. `CrowdManagerClass` is a config property (`NavigationSystem.h:318–319`) and an empty class means none is created (`NavigationSystem.cpp:1570–1579`). **Found in S1:** with the default class, the manager looks for a Recast navmesh during world teardown, finds the runtime one already gone, and logs `LogCrowdFollowing: Warning: Unable to find RecastNavMesh instance …` (`AIModule/Private/Navigation/CrowdManager.cpp:905`) on every exit. This project uses no crowd-following agents |
| `DefaultEngine.ini` map comment | Explains that the arena is built at runtime, 1 km from the template's floor |
| `DefaultGame.ini` `+DirectoriesToAlwaysCook=(Path="/Engine/BasicShapes")` | The cube is referenced only from C++, never from a map, so the cooker wouldn't include it otherwise (the same rule UE5-UpdateBudget found) |

### B6. `Tools\`: running and checking the packaged build

> `run_packaged.bat` and `s1_checks.bat` were **replaced in S2** by `run_game.bat` and `s2_checks.bat` (C6). The exit-code rule below applies to every check script.

| Script / change | Key lines | Why |
|---|---|---|
| `run_packaged.bat <LogName> [nullrhi\|gpu] ["game args"]` | `"%EXE%" %RHI% -nosound -unattended -traceautostart=0 -log -abslog="%LOG%" %~3` | Runs the packaged game once for a functional check. **The game arguments are one quoted string:** batch splits unquoted arguments at `=`, so `-NRTimeout=30` would reach the game as two words (code-review finding); `%~3` strips the quotes and the game receives the text unchanged (tested: `['-NRSpike', '-NRTimeout=30']`). The exit code is the game's own |
| `s1_checks.bat` | build editor, build game, package, spike without and with the GPU; `:step` prints timestamps | All S1 checks in one queue job |
| **Every check script:** `if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%` after each `call :step` | replaces `call :step … \|\| exit /b` | **Found in S1:** under `cmd /c` (how the queue runs scripts), `exit /b` with no number returned **0**: a job whose build failed was recorded as passed (reproduced: 0 instead of 6). `if errorlevel 1` would fix that but misses a crash, whose NTSTATUS code is negative (code-review finding; reproduced: `-1073741819` now stops the script). `phase4_checks.bat` had the same latent bug; its Phase 4 results were real passes (every step printed `exit=0`) |

---

## Part C · Step S2: the content and the determinism hash

**What S2 adds:** the real workload (16 cells, two prop meshes, seeded variation, a cruise-then-burst camera path), the plugin's first code (the content hash, with unit tests), a check that no cell encloses an unreachable pocket, and a run report. **What S2 removes:** the S1 spike checks; their results are recorded in `09` S1, and the director's run mode subsumes them (it waits for a navigable arena before the camera moves).

**Files:** plugin `Public\NavReadyContentHash.h` + `Private\NavReadyContentHash.cpp` (C1), `Private\Tests\NavReadyTestFlags.h` + `NavReadyContentHashTests.cpp` (C2); game `Private\NRGraphFactory.h/.cpp` (C3), `Public\NRBenchmarkSubsystem.h` + `Private\NRBenchmarkSubsystem.cpp` (C4), `NavReadyPCG.Build.cs` (C5); `Tools\run_game.bat`, `run_tests.bat`, `s2_checks.bat`, `compare_hashes.py` (C6).

### C1. `NavReadyContentHash.h` / `.cpp` (plugin): one hash per cell

| Line | What it does, and why |
|---|---|
| `#define UE_API NAVREADYPCGCORE_API` … `#undef UE_API` | 5.8's export convention (`01b` of UE5-UpdateBudget §3): every exported function is marked `UE_API`, and the macro is undefined at the end so it can't leak into other headers |
| `struct FNavReadyCellHash { ContentHash, OrderedHash, NumInstances }` | Plain data, no UObject: it is copied around freely and compared with the defaulted `operator==` (C++20) |
| `ContentHash` | **Order-independent:** the same set of instances in any order gives the same value. This is what SC2 compares |
| `OrderedHash` | **Order-dependent:** also changes when the same instances come out in a different order. Reported to test hypothesis H6 (PCG's multithreaded execution may change instance order); not a pass/fail criterion |
| `LocationStep = 0.1`, `RotationStep = 1e-4`, `ScaleStep = 1e-3` | The quantization steps of `02` §7: 1 mm; 1e-4 per quaternion component (about 0.01°); 1e-3 of scale |
| `HashInstance(FStringView MeshPath, const FTransform&)` | `Quantize(V, Step) = RoundToDouble(V / Step)` as `int64` for the 3 location, 4 rotation and 3 scale values. **All ten are `int64`, so the array has no padding bytes** and `FCrc::MemCrc32` over its memory is deterministic (a struct mixing `uint32` and `int64` would contain uninitialised padding) |
| Quaternion sign | `q` and `-q` are the same rotation. One representative is chosen **after** quantization (W positive; if W rounds to 0, the first non-zero of X, Y, Z positive), so a component that rounds to zero can't flip the choice |
| `FCrc::StrCrc32Len(MeshPath.GetData(), MeshPath.Len())` as the seed | The mesh's **path string** is hashed, never an `FName` index or a pointer (both change between runs). `StrCrc32Len`, not `StrCrc32`: a string view isn't null-terminated, and `StrCrc32`'s second parameter is a seed, not a length (`Core/Public/Misc/Crc.h:43, 65`) |
| `Combine(TConstArrayView<uint32>)` | `OrderedHash` = CRC of the instance hashes as given; `ContentHash` = CRC of the same hashes **sorted** (a multiset: duplicates still count) |
| `HashComponents(…)` | Takes the cell's generated ISM components, orders them by mesh path (`StableSort`, so ties keep their input order), reads every instance's **world** transform (`GetInstanceTransform(i, T, true)`), hashes and combines. Components are ordered because PCG's creation order isn't part of the content |
| *Known limit* | Quantization ignores float noise **unless** a value sits within the noise of a rounding boundary; then one prop can still flip the hash. If SC2 ever fails, the order-dependent and per-cell values in the run report show which cell and whether the count changed |

### C2. `NavReadyTestFlags.h`, `NavReadyContentHashTests.cpp` (plugin): unit tests

Pure tests on hand-made transforms: no world, no PCG. Run with `Tools\run_tests.bat NavReadyPCG.Hash` (the editor, headless), the same mechanism as UE5-UpdateBudget.

| Test | What it proves |
|---|---|
| `NavReadyPCG.Hash.OrderIndependent` | Reversing three instances keeps `ContentHash` and changes `OrderedHash` |
| `NavReadyPCG.Hash.IgnoresFloatNoise` | 1e-5 cm and 1e-7 of scale (far below the steps) don't change the hash. The test values sit well away from rounding boundaries |
| `NavReadyPCG.Hash.SeesRealChanges` | A 1 cm move, a 1° turn, a scale change, another mesh, or a missing prop each change the hash |
| `NavReadyPCG.Hash.QuaternionSign` | `q` and `-q` hash the same |
| `NavReadyPCG.Hash.Empty` | An empty cell has a stable hash, different from a cell with one prop |

`EAutomationTestFlags::EditorContext | ClientContext | ProductFilter`: run in the editor and the game, under the "Product" filter; the macro requires exactly one filter flag. Hashes are `uint32`, and `TestEqual` has no `uint32` overload (the `int32`/`int64`/`SIZE_T`/`float`/`double` overloads make the call ambiguous, `Core/Public/Misc/AutomationTest.h:1985–1989`), so equal hashes are compared as `int64`, which still prints both values on a failure. `TestNotEqual` is a template and takes `uint32` directly (`:2412`).

### C3. `NRGraphFactory` (game): Transform Points and two meshes

| Change | Why |
|---|---|
| `VoxelSize = (400, 400, 1000)` | 4 m spacing, one layer (the 1 m tall box holds one Z lattice index). The sampler's lattice is **world-aligned**: points sit at whole multiples of the voxel size (`PCG/Private/Elements/PCGVolumeSampler.cpp:65–70, 139`), so a 29.64 m cell holds 7 or 8 points per side: 49, 56 or 64 props per cell, 900 in all (`09` S2 confirms) |
| `UPCGTransformPointsSettings` (`MinimalAPI`): `OffsetMin/Max = ∓(25, 25, 0)`, `RotationMax = (0, 360, 0)`, `ScaleMin/Max = 0.9 / 1.4`, `bUniformScale = true` | Seeded per-point jitter, yaw and uniform scale. **Why these ranges:** the engine shapes' pivot is their centre (the director logs each mesh's bounds, `09` S2) and the points lie on the floor, so a prop stands 50 cm × scale above it; 0.9 keeps the smallest at 45 cm, above the 35 cm step height (`BaseEngine.ini:3056`), so every prop blocks. The offset isn't scaled (`bAbsoluteOffset` false: it is only rotated, `PCGTransformPoints.cpp:207–215`), so ±25 cm keeps lattice neighbours ≥ 3.5 m apart: the narrowest gap, between two 1.4 m cubes corner to corner, is about 1.5 m; a gap closes in the navmesh below about 1.15 m, because the agent's 34 cm radius rounds up to two 19 cm voxels on each side (`RecastNavMeshGenerator.cpp:5260`) and rasterization can add up to a voxel more on each side **[inferred]**. The first S2 run used ±50 cm (a 1 m worst gap) and found one enclosed sample point (`09` S2). Each point's seed is computed from its lattice indices (`PCGVolumeSampler.cpp:157`); Transform Points draws from `FRandomStream(ComputeSeed(Seed, PointSeed))`, where `Seed` combines the node's seed with the component's (`PCGTransformPoints.cpp:70, 170`, `PCGSettings.cpp:723–726`). So a cell's content is fixed by its seed and position, and changes with the component seed (checked in S2) |
| Mesh entries: cube and cylinder, weight 1 each | The weighted selector picks a mesh per point from a stream seeded the same way: point, node and component seed (`MeshSelectors/PCGMeshSelectorWeighted.cpp:228–230`, `Helpers/PCGHelpers.cpp:55–61`) |
| Wiring check covers all three target pins | As in S1: the edge functions don't report success |
| *Dropped:* the "thinning to about half" of `01` §6.1 | A thinning node adds a moving part and its own seed; the same density is set more simply by the voxel size (`02` §14) |

### C4. `NRBenchmarkSubsystem` (game): the director, S2 version

| Part | What it does, and why |
|---|---|
| `-NRRun`, `-NRSeed=`, `-NRRunTag=`, `-NRTimeout=` (default 90), `-NRExit` | Run mode replaces S1's `-NRSpike` |
| `#include "NavReadyContentHash.h"` in the public header | `FCellRecord` stores an `FNavReadyCellHash` by value, so the header needs the type. A public header now includes a plugin header, so the plugin becomes a **public** dependency (C5) |
| `struct FCellRecord { Grid, Seed, GenerationStart, GenerationDone, Hash }` | One record per cell, in a plain `TArray` (no UObject, so no `UPROPERTY`); the cell actors themselves are in a `UPROPERTY` array so GC sees them |
| `enum class EPhase { Idle, WaitingForNavmesh, Flying, Settling, Done }` | The run's timeline. `Settling` exists for the pocket check, which needs the navmesh after every tile has been rebuilt |
| `NavTileSize = 988`, `ArenaCenter = (101 × 988, 988, 0)`, `CellSize = 3 × 988` | The engine's navmesh tiles are not 10 m: the generator truncates `TileSizeUU` (1000) to whole voxels, `TruncToInt(1000 / 19) = 52` voxels × 19 cm = **988 cm** (`NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp:5270`, `Public/NavMesh/RecastNavMeshGenerator.h:129`, `RecastNavMesh.cpp:1894–1899`), and the tile grid starts at the world origin unless `NavMeshOriginOffset` is set (`RecastNavMesh.h:788–790`; it becomes the Detour origin, `RecastNavMeshGenerator.cpp:5623–5627`). So a 30 m cell can't line up with tiles; a 29.64 m cell (3 × 3 tiles) on a tile boundary does, and no tile lies in two cells |
| Mesh bounds line in `BuildArena` | Logs each prop mesh's Z extent around its pivot (`UStaticMesh::GetBoundingBox`): the fact the scale range depends on (C3), recorded in every run |
| `NRRun: pocket point in cell …` log line | Where an enclosed point is, so a pocket can be looked at instead of only counted |
| Lattice check in `SpawnCells` | A cell edge exactly on the sampler's 4 m lattice would give the points on that line to **both** cells: two overlapping props. With the arena centred on y = 0, the edge y = 0 did exactly that, so the arena moved one tile off the axis; every edge is now at least 24 cm from a lattice line. The check fails the run if a changed constant brings an edge within 1 cm of the lattice |
| `AreTilesAlignedWithCells()` | At warm-up, compares the real navmesh's `GetTileSizeUU()` and `NavMeshOriginOffset` with those assumptions and fails the run if they differ, so a changed navmesh setting can't silently break per-cell readiness |
| `SpawnCells`: 4 × 4 cells, seed `BaseSeed * 100 + Index` | A different base seed changes every cell's seed |
| `AddUObject(this, &HandleCellGenerated, Index)` | A **payload** argument: the delegate's own parameter (`UPCGComponent*`) comes first, then the bound `Index`, so the handler knows which cell fired without searching |
| `NRBench::ArenaCorner(Sign)`, `NRBench::GetNavigation(World, OutNavSys)` | One definition each of the arena corner (5 m inside the edges, 50 cm up) and of the navigation lookup, shared by the warm-up path, the tile check, the idle check and the pocket check |
| `IsArenaNavigable()` (warm-up gate) | The camera holds still until a corner-to-corner path is complete, so generation never races the arena's initial navmesh build |
| `NRBench::CameraX(FlightSeconds)` | The path: from x = −110 m at 4 m/s to −50 m (cruise: grid columns come into range one at a time), then 12 m/s to +110 m (burst: the last two columns come into range within about 4 s). Along the arena's X axis through its centre, 15 m up. The scheduler measures from the source to the cell's **bounds** in 2D (`PCG/Private/RuntimeGen/PCGRuntimeGenScheduler.cpp:742–751`; 2D because a graph's `bUse2DGrid` defaults to true, `PCG/Public/PCGGraph.h:728`, read through `PCGComponent.cpp:172–180`), so the outer rows (nearest edge 29.64 m from the path) are reached too |
| Times in `FCellRecord` | Wall-clock seconds since the flight started (`FPlatformTime::Seconds()`), because PCG's budget and tile builds run in real time (`01b` B-1) |
| `HandleCellGenerated` → `NavReadyContentHash::HashComponents` | The hash is taken the moment PCG reports the cell generated |
| `IsNavigationIdle()` | `!HasDirtyAreasQueued() && !IsNavigationBuildInProgress()`: nothing waiting to be marked dirty, and no generator reports a build in progress (`NavigationSystem/Private/NavigationSystem.cpp:3561–3564, 4889–4911`). Global, so fine for a settled world at the end of a run; a cell's readiness (S3) must not use it (`07`) |
| `Settling`, then `CheckPockets()` | At least 1 s after the last cell generated (`SettleSeconds`: its props reach the navigation octree in a later navigation tick), then until navigation has been idle for **two frames in a row** (`RequiredIdleFrames`: one idle frame can be a gap between tile tasks, as in S1). Then, per cell, a 14 × 14 grid of points is projected onto the navmesh from 10 cm above the floor in a ±5 cm × ±20 cm box: that finds the floor's navmesh, never the tops of props (45 cm and taller), and fails where a prop stands. Every point that projects gets a `FindPathSync` from the arena corner; an incomplete or partial path is a **pocket point**. Pockets would make agents fail for a reason that has nothing to do with stale navigation, so S2 requires zero (`03` S2) |
| `FinishRun(Reason, bCompleted)` | Every end of the run goes through it: set-up failure, no navmesh, tiles that don't match, timeouts, success. The run passes only if `bCompleted`, every cell generated, **every cell has props and at least one blocked sample point** (so a run whose props lost their collision or navigation flag fails instead of measuring nothing), and no cell started generating before the flight (its times would be meaningless) |
| Order in `SpawnCells` | The record and the cell are added **before** the delegates are bound and `FinishSpawning` runs, so a handler can never index past them |
| `FMath::Clamp(BaseSeed, 0, 1000000)`, `FPaths::MakeValidFileName(RunTag, '_')` | The seed keeps `BaseSeed × 100 + index` far inside `int32`; the tag names the report file, so it may not contain folders or `..` (`Core/Public/Misc/Paths.h:696`) |
| End of run | After the pocket check, or at the timeout. `FinishRun` logs one line per cell and a summary with a **run content hash** (the cell hashes chained in cell order with `FCrc::TypeCrc32`: one value to compare runs by), then writes `Saved/NRRuns/<RunTag>.csv` |
| Exit | As in S1: a clean exit on success, a forced exit with code 1 on failure |

### C5. `NavReadyPCG.Build.cs`

`NavReadyPCGCore` moves from private to **public**: `NRBenchmarkSubsystem.h` (a public header) now includes `NavReadyContentHash.h`. The rule from `06` §2, applied header by header.

### C6. `Tools\` (S2)

| Script | Why |
|---|---|
| `run_game.bat <Log> <nullrhi\|gpu\|uncooked> ["args"]` | Replaces S1's `run_packaged.bat` and adds `uncooked` mode (the editor binary in `-game`), needed to compare editor and packaged content. The game arguments are one quoted string (S1 code-review finding). **Each launch is on its own line, outside any `( )` block** (S2 code review): cmd expands `%ERRORLEVEL%` in a block when it reads the block, so an `exit /b %ERRORLEVEL%` inside one returned the code from before the game ran (reproduced: 0 instead of 3). An older log is deleted first, so a run that never starts can't leave stale results for `compare_hashes.py` |
| `run_tests.bat [Filter]` | The editor, headless: `-ExecCmds="Automation RunTests <Filter>;Quit"`. The automation command splits its own text at `;` (`Developer/AutomationController/Private/AutomationCommandline.cpp:582`), and its `Quit` exits with **−1** if any test failed, else 0, through a forced exit (`:491–504`). Adapted from UE5-UpdateBudget |
| `s2_checks.bat` | Builds, unit tests, package, then five runs (seed 1 twice, seed 2, seed 1 on the GPU, seed 1 uncooked) and the comparison, stopping at the first failure |
| `compare_hashes.py <folder>` | Reads each run's per-cell log lines and checks SC2 as far as S2 can: identical content across runs, RHIs and builds for the same seed; a different seed changes every cell; every cell generated; no pocket points in any run. Exit 1 on failure. Self-tested on synthetic logs (pass, one changed cell, one pocket) |
| *Removed:* `s1_checks.bat`, `run_packaged.bat` | Their job (the S1 spike) is done and recorded in `09`; git history keeps them |

---

## Part D · Step S3: the readiness observer and the measurement harness

**What S3 adds:** the plugin's tracking code (a cell's state, the readiness rule, the registry and the per-frame probe), connected to the director's cells, and the scripts that run and summarise benchmark batches. **What S3 does not do yet:** change anything. NavReady only **observes**: in the baselines the props reach navigation when PCG spawns them, so a generated cell is marked committed without touching it, and the probe reports when its tiles are rebuilt. That moment is M1 (time to navigable), measured with the same code that will drive NavReady in S5.

**Files:** plugin `Public\NavReadyCell.h` + `Private\NavReadyCell.cpp` (D1), `Public\NavReadyCellComponent.h` + `.cpp` (D2), `Public\NavReadySubsystem.h` + `.cpp` (D3), `Private\Tests\NavReadyCellTests.cpp` (D4); game `NRCellActor` and `NRBenchmarkSubsystem` (D5); `Tools\csv_stats.py`, `run_bench.py`, `check_probe.py`, `s3_checks.bat`, `cpu_affinity.py` (D6).

### D1. `NavReadyCell.h` / `.cpp` (plugin): the state and the rule, as plain data

| Line | What it does, and why |
|---|---|
| `UENUM(BlueprintType) enum class ENavReadyCellState : uint8 { Idle, Generating, Generated, Committed, AIReady }` | One vocabulary for C++, Blueprint (`OnCellStateChanged`), the CSV and the debug view. A `UENUM`, so this header has a `.generated.h` |
| `struct FNavReadyProbe { bNavigationAvailable, bBuildLocked, bDirtyAreasQueued, bCellHasDirtyTiles }` | What navigation reported this frame for one cell. Plain booleans: the subsystem fills them from the engine, the tests write them by hand |
| `struct FNavReadyCell` | State, readiness bounds, four timestamps, the commit frame, and the "dirtied while ready" counter. No UObject, so the transitions run in unit tests without a world (`02` §4.1) |
| `OnGenerationStarted(Now)` | Any state → `Generating`, and the previous generation's times are cleared (a regenerated cell starts over) |
| `OnGenerated(Now)`, `OnCommitted(Now, Frame)` | Only from `Generating` and `Generated` respectively; both return `false` otherwise, so a stray or late event (a generated event after a cancel) can't skip a state (S3 code review) |
| `int32 Id` | Given at registration, in registration order, and kept through resets. The CSV events use it, because registry slots move when a cell unregisters |
| `TrySettle(Probe, Now, Frame)` | `Committed` → `AIReady` when `IsSettled`; returns `true` on that transition only, so the caller broadcasts once |
| `IsSettled`: `bNavigationAvailable && Frame > CommitFrame && !bBuildLocked && !bDirtyAreasQueued && !bCellHasDirtyTiles` | The readiness rule of `02` §4.1. `Frame > CommitFrame`: navigation ticks before the tick groups (`LevelTick.cpp:1647`) and registers new content in that tick, so nothing committed in frame N can be seen before frame N + 1 |
| `ObserveWhileReady(Probe)` | Counts each **episode** of dirty tiles in a cell that is already ready (rising edges only). S3 measures how often a neighbour's edge props re-dirty a ready cell, to decide S5's policy with data |
| `OnReset()` | Cleanup or cancellation: back to `Idle`; the id and the bounds are kept (they belong to the cell, not to one generation) |
| `#define UE_API NAVREADYPCGCORE_API` on each member | 5.8's export convention, as in `NavReadyContentHash.h` |

### D2. `NavReadyCellComponent.h` / `.cpp` (plugin): the cell

| Line | What it does, and why |
|---|---|
| `UCLASS(MinimalAPI, ClassGroup = (NavReady), meta = (BlueprintSpawnableComponent))` | Exported type info only; the members other modules call carry `UE_API`. Spawnable, so a Blueprint actor can carry it |
| `PrimaryComponentTick.bCanEverTick = false` | The subsystem does every cell's per-frame work in one loop |
| `BeginPlay`: `GetWorld()->GetSubsystem<UNavReadySubsystem>()`, else return | The subsystem exists only in game and PIE worlds, so in an editor world the component does nothing |
| `FindComponentByClass<UPCGComponent>()` | The owner's PCG component. Missing → an error and the cell is ignored |
| Four `AddUObject` bindings: start, generated, cancelled, cleaned | PCG's native delegates (`PCGComponent.h:55–58`, members `:380–383`); cancelled and cleaned both reset the cell |
| `EndPlay`: `RemoveAll(this)` on all four, then `UnregisterCell` | No dangling bindings; the registry never holds a dead cell (`02` §8) |
| `GetState()`, `IsAIReady()`, `GetCell()` | Read through the subsystem's registry, so there is one copy of the state |

### D3. `NavReadySubsystem.h` / `.cpp` (plugin): the registry and the probe

| Line | What it does, and why |
|---|---|
| `static_assert(WITH_RECAST, …)`, `static_assert(RECAST_ASYNC_REBUILDING, …)` | The probe reads Recast's dirty tiles; every desktop target compiles Recast (`Target.bCompileRecast`, `NavigationSystem.Build.cs:40–43`). And `HasDirtyTiles(FBox)` sees pending and running tiles only, while a tile in the middle of a **time-sliced** rebuild is in neither (the unbounded `HasDirtyTiles()` does count it, `RecastNavMeshGenerator.cpp:6588–6593` vs `:6644–6670`). Time slicing is used only without asynchronous rebuilding, which is the default (`NavMesh/RecastNavMesh.h:28–32`), so the probe requires it (S3 code review) |
| `TArray<FNavReadyCell> Cells` + `UPROPERTY TArray<TObjectPtr<UNavReadyCellComponent>> Components` | Parallel arrays: the hot loop reads plain data; the UObject array keeps the components visible to GC |
| `RegisterCell` / `UnregisterCell` | Append; remove with `RemoveAtSwap`. During a broadcast the slot is only emptied, and `Compact()` removes it afterwards, so a listener that destroys a cell can't shift the arrays under the loop |
| `Notify…` (from the component) | Run the transition and queue a broadcast. They arrive from PCG's tick, outside NavReady's |
| `Tick`: `CSV_SCOPED_TIMING_STAT_EXCLUSIVE(NavReady)` | NavReady's own cost as `Exclusive/GameThread/NavReady`, subtracted from `Tickables` automatically (M6) |
| The probe's global part, once per frame: `GetDefaultNavDataInstance(DontCreate)` → `Cast<ARecastNavMesh>` → `static_cast<const FRecastNavMeshGenerator*>(GetGenerator())` | Fetched every frame, never cached: a full rebuild can recreate the generator (`NavigationData.cpp:626`). The `static_cast` is safe only after the `ARecastNavMesh` check |
| `IsNavigationBuildingLocked()`, `HasDirtyAreasQueued()` | Global, as `02` §12 #2 states (`NavigationSystem.h:1083`, `NavigationSystem.cpp:3561–3564`) |
| A `Generated` cell, once a generator exists: readiness bounds = the owner's components bounds grown by the navmesh border, then `OnCommitted` | Every tile a change in the cell can mark dirty, including a neighbour's edge tile. The engine grows each dirty area by (high, low) border before marking tiles (`GrowDirtyBounds`, `RecastNavMeshGenerator.cpp:6603–6608`, used at `:6787`; not exported); the exported `GrowBoundingBox` (`:6596–6601`, `.h:836`) grows by (low, high). So the code takes the **larger growth per axis on both sides**, which covers either. Without a generator the cell waits in `Generated`: bounds taken without the border would stay too small for good (S3 code review) |
| A `Committed` or `AIReady` cell: `HasDirtyTiles(ReadinessBounds)` | Pending or running tile builds inside the box (`RecastNavMeshGenerator.cpp:6644–6670`). A box edge exactly on a tile boundary counts the lower tile only (`FRcTileBox`, `.h:240–257`) |
| `CSV_EVENT(NavReady, "CellCommitted_%d")`, `"CellReady_%d"` | Frame-exact markers in the capture, used by `check_probe.py`, named by the cell's stable `Id` (cells register in spawn order, so the id equals the director's cell index) |
| `CSV_CUSTOM_STAT(NavReady, CellsGenerating / CellsGenerated / CellsCommitted / CellsReady / CellsDirtyWhileReady)` | Per-frame counts for the timeline charts |
| `FlushBroadcasts()` | At the end of the tick, by index and by copy (a listener can queue more), weak pointers (a listener can destroy a cell) |

### D3b. `NavReadyPCGCore.Build.cs`: the `Navmesh` module

`PrivateDependencyModuleNames.Add("Navmesh")`. The probe includes `NavMesh/RecastNavMeshGenerator.h`, which includes Recast's own headers (`Recast/Recast.h`). Those belong to the `Navmesh` module, which `NavigationSystem` depends on **privately** (`NavigationSystem.Build.cs:42`), so depending on `NavigationSystem` doesn't make them visible. The first S3 build stopped on exactly this (`09` S3, `07`).

### D4. `NavReadyCellTests.cpp` (plugin)

| Test | What it proves |
|---|---|
| `NavReadyPCG.Cell.NotReadyInCommitFrame` | Never ready in the commit frame, however clear the probe; ready the next frame; only once |
| `NavReadyPCG.Cell.NotReadyWhileBusy` | Not ready with no navmesh, a lock, queued dirty areas, or dirty tiles in the cell; ready once all clear |
| `NavReadyPCG.Cell.Transitions` | Only a generated cell commits; regeneration clears the times; reset keeps the bounds |
| `NavReadyPCG.Cell.DirtyWhileReady` | Counts episodes, not frames |

### D5. Game: `NRCellActor` and `NRBenchmarkSubsystem` (S3 changes)

| Change | Why |
|---|---|
| `ANRCellActor`: a `UNavReadyCellComponent` subobject, `GetNavReady()` | Every cell is tracked; the component finds the PCG component itself |
| `-NRConfig=` (only `B0` in S3) and `-NRCells=K` (1–16) | The configuration is recorded in the report and the CSV metadata. One cell isolates one cell's navigation work for the probe check |
| Warm-up waits for 2 idle navigation frames as well as a complete path | The arena's first build must be finished before any cell starts, or the first cells' M1 would include it |
| `CSV_METADATA(NRRunTag / NRConfig / NRSeed)`, `CSV_EVENT(NRBench, "BenchStart")` at the flight's start, `"BenchEnd"` in `FinishRun` (only if the flight started; otherwise the NavReady times stay −1) | The capture covers exactly the flight and the settling; `-csvStartOnEvent` / `-csvStopOnEvent` match the event text (`CsvProfiler.cpp:4759, 4769`) |
| Settling also waits for every cell to be `AIReady` | NavReady's tick and the director's have no fixed order; without this, the last cell could still read `Committed` on the frame navigation went idle |
| `FCellRecord`: `CommitTime`, `ReadyTime`, `DirtiedWhileReady`, copied from `FNavReadyCell` in `FinishRun` | One run report with generation and readiness, all relative to the flight's start |
| The run fails if a cell never reached ready | M1 must exist for every cell |

### D6. `Tools\` (S3)

| Script | Why |
|---|---|
| `csv_stats.py` | Adapted from UE5-UpdateBudget: reads a finalised Unreal CSV capture (end header and metadata row), percentiles by linear interpolation, event parsing. Default columns are this project's (frame times, the three exclusive stats, `NavReady/CellsReady`). `--selftest` checks hand-worked values |
| `run_bench.py` | Real time capped at 60 fps (`t.MaxFPS`), not a fixed step (`02` §14). `--cells` must be 1–16 and `--label` may hold only letters, digits, `-` and `_` (it becomes part of file names and `-NRRunTag`). Adapted from UE5-UpdateBudget: pins itself to the P-cores once (`cpu_affinity.pin_self`), records the environment with the mask read back, one priming run, interleaved runs, the real executable (not the bootstrap) through `cpu_affinity.run_game` (Popen, timeout, kill in `finally`), artefacts moved to `Benchmarks\Raw` without overwriting, manifest after every run, summary per configuration (M1 and settle per cell, frame statistics, NavReady's cost) |
| `check_probe.py` | The S3 agreement check on a one-cell capture: the frame NavReady reports ready vs. the frame the engine's remaining tile tasks return to 0 after the commit. Tasks already running **in** the commit frame count (in the baselines the props reach navigation while PCG spawns them, before the commit). `--selftest` covers agreement, tolerance, early and late readiness, "no tasks seen", and tasks only in the commit frame |
| `s3_checks.bat` | Builds, every `NavReadyPCG.*` test, package, the self-tests, a B0 batch (priming + 1), the one-cell run, and the probe check, stopping at the first failure. `:step` collects its arguments with `shift`: `%2 … %9` reach only eight, and the benchmark steps have more (S3 code review; tested with ten) |
| `cpu_affinity.py`: `run_game` takes a string as well as a list | A string goes to `CreateProcess` as written, which keeps quoted arguments intact |

---

## Part E · Step S5 (final, smallest form): the NavReady commit

**Scope (owner's decision, 2026-11-21):** finish the core feature in its smallest working form. NavReady now **commits** each generated cell itself, hashes it when it settles, and marks it AI-ready; the director runs it as configuration `NR`. Dropped: the measuring agents (M2, M4, M4b), the baselines B1–B3, the debug view and the commit-budget sweep (`03`, `09`).

**Files:** plugin `NavReadyCell.h` and `NavReadySubsystem.cpp` (E1); game `NRBenchmarkSubsystem` (E2); `Tools\run_bench.py`, `s5_checks.bat` (E3).

### E1. The commit and the hash (plugin)

| Line | What it does, and why |
|---|---|
| `TAutoConsoleVariable<int32> CVarCommitsPerFrame(TEXT("nr.CommitsPerFrame"), 1, …, ECVF_Default)` | At most this many cells are handed to navigation per frame (at least 1). `ECVF_Default`, because cheat cvars can't be set from config or `-dpcvars` |
| `NavReadyPrivate::GetGeneratedComponents` | The owner's ISM components that carry PCG's tag (`PCGHelpers::DefaultPCGTag`): exactly what PCG generated |
| A `Generated` cell, a generator, and budget left: `SetCanEverAffectNavigation(true)` on every generated component that can't yet affect navigation, then `OnCommitted` | **The commit.** In `NR` the spawner created the props navigation-invisible, so navigation saw nothing while the cell was generating; now the whole cell becomes navigation-relevant in one frame, and navigation rebuilds its tiles once. In `B0` the props already affect navigation, so there is nothing to switch and the commit only marks the moment, as in S3 |
| `NavSys->AddDirtyArea(Instances->Bounds.GetBox(), ENavigationDirtyFlag::All, …)` after the switch | **Found in the final batch:** the switch alone left NR's props invisible to the navmesh (0 of 16 cells blocked). An ISM opts out of the dirty area when it is added to the navigation octree, because it expects per-instance updates (`InstancedStaticMesh.cpp:5513–5519`, honoured at `NavigationDataHandler.cpp:50`); a component that already has its instances therefore never dirties its tiles. The commit now dirties each component's bounds itself. **Not yet verified by a run** (`09`) |
| `CommitsThisFrame` CSV stat | How many cells were committed in each frame |
| On `TrySettle`: `Cell.Hash = NavReadyContentHash::HashComponents(…)` | Settle → hash → AI-ready (`02` §2): the hash describes exactly the content the AI is about to use |
| `FNavReadyCell::Hash` | Stored with the cell; the director compares it with its own hash taken at generation |

### E2. The director (game)

| Change | Why |
|---|---|
| `-NRConfig=NR` accepted | `B0` or `NR` |
| `GraphParams.bNavigationVisible = Config != TEXT("NR")` | The one content difference between the configurations: the spawner's navigation flag, which the hash ignores (`01` §6.2) |
| `FCellRecord::bHashMatchesAtReady` and the summary line | NavReady's hash at AI-ready must equal the director's hash at generation in every cell |

### E3. Tools

| Script | Why |
|---|---|
| `run_bench.py`: `CONFIGS` `B0` and `NR` | The short comparison batch |
| `s5_checks.bat` | Builds, every `NavReadyPCG.*` test, package, and `run_bench.py --configs "B0,NR" --runs 2` (priming + 2 interleaved runs each). The configuration list is **quoted**: cmd also splits arguments at commas, and the first final job stopped on exactly that (`07`) |
