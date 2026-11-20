# 05 · Line By Line Code Explanation: NavReadyPCG

**Rule:** every file that exists in `NavReadyPCG\` and `Tools\` is explained here, line by line, in the same step it is written. Sections follow the build order in [03_Implementation_Roadmap.md](03_Implementation_Roadmap.md).

| Part | Step | Files |
|---|---|---|
| A | Phase 4, F1–F3 (foundation) | `.uproject`, two `Target.cs`, two `Build.cs`, two module `.cpp`, `.uplugin`, three config `.ini`, `Tools\*.bat`, `Tools\cpu_affinity.py` |
| B onwards | S1–S7 | written with each step |

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
