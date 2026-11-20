# 06 · Unreal APIs And Macros: NavReadyPCG

**Rule:**
- Only APIs this project **actually uses** appear here, each in the same step it enters the code.
- Each entry follows the 8-point format: what it is · why it exists · how it works · when to use it · when not to · common mistakes · a small example · how we use it.
- Verified against UE **5.8.4** source; file references are relative to `Engine/`.
- The APIs planned for S1–S5 (PCG components and delegates, the navigation system's dirty-area and tile queries, `SetCanEverAffectNavigation`, world subsystems, CSV stats) are listed with their evidence in [01b](01b_Research_Findings.md) §2–3 and [02](02_System_Architecture.md) §4. Each gets a full entry here when it enters the code.

| # | API | First used in |
|---|---|---|
| 1 | `IMPLEMENT_PRIMARY_GAME_MODULE` / `IMPLEMENT_MODULE`, `FDefaultGameModuleImpl` / `FDefaultModuleImpl` | Phase 4 |
| 2 | `ModuleRules`: dependency lists and `PCHUsage` | Phase 4 |
| 3 | `TargetRules`: `Type`, `DefaultBuildSettings`, `IncludeOrderVersion` | Phase 4 |
| 4 | Descriptor `LoadingPhase` (`.uproject` / `.uplugin`) | Phase 4 |
| 5 | Plugin dependencies in descriptors (`"Plugins"` in `.uplugin` and `.uproject`) | Phase 4 |
| 6 | `-ExecCmds=` and reading console variables from the log | Phase 4 (smoke scripts) |

---

## 1. `IMPLEMENT_PRIMARY_GAME_MODULE` / `IMPLEMENT_MODULE`

1. **What it is.** Macros that generate a module's entry point (`Runtime/Core/Public/Modules/ModuleManager.h:946–955` for `IMPLEMENT_MODULE`, `:984` for `IMPLEMENT_GAME_MODULE`, `:1100–1136` for the primary variant).
2. **Why it exists.** The engine loads code as **modules**: DLLs in editor builds, static code inside one executable in packaged builds. Each module needs a standard way to be found and started, so the module manager can call its `StartupModule()`/`ShutdownModule()`.
3. **How it works.**
   - In a modular (DLL) build it exports an `InitializeModule` function, which the module manager calls after loading the DLL; it returns an instance of the class you name.
   - In a monolithic build it registers a static initializer instead.
   - The *primary* variant also sets up project-wide globals for packaged builds and key/target-name registration. Its third argument is named `DEPRECATED_GameName` in the monolithic overloads (`:1100, :1114`): it is ignored.
4. **When to use it.** Exactly one `IMPLEMENT_MODULE` (or a game variant) per module; exactly one `IMPLEMENT_PRIMARY_GAME_MODULE` per project.
5. **When not to.** Not twice in one module (duplicate symbols at link time). A plugin module uses `IMPLEMENT_MODULE`, never the primary game variant.
6. **Common mistakes.**
   - The module-name argument doesn't match `Build.cs` and the descriptor ("module not found" at start-up).
   - Writing an empty `IModuleInterface` subclass when `FDefaultModuleImpl` would do.
   - Believing the third argument still sets the game name.
7. **Example.**
   ```cpp
   #include "Modules/ModuleManager.h"
   IMPLEMENT_MODULE(FDefaultModuleImpl, MyPluginModule);
   ```
8. **In this project.** `NavReadyPCGModule.cpp` uses the primary variant with `FDefaultGameModuleImpl`; `NavReadyPCGCoreModule.cpp` uses `IMPLEMENT_MODULE(FDefaultModuleImpl, NavReadyPCGCore)`. Neither needs start-up logic. The smoke log shows the plugin DLL loading: `LogModuleManager: InternalLoadLibrary: 'NavReadyPCGCore'` (`09` F-3).

## 2. `ModuleRules`: dependencies and `PCHUsage`

1. **What it is.** The C# class in `<Module>.Build.cs` that tells UnrealBuildTool how to compile one module.
2. **Why it exists.** Unreal is hundreds of modules. Each declares what it depends on, so UBT can work out include paths, link order and what to rebuild.
3. **How it works.**
   - `PublicDependencyModuleNames`: their include paths are given to us **and** to every module that depends on us.
   - `PrivateDependencyModuleNames`: only to us.
   - Both link the dependency.
   - `PCHUsage = UseExplicitOrSharedPCHs` uses a shared precompiled header built for a common dependency set, unless the module names its own.
4. **When to use which.** Public **only** if one of *your public headers* includes that module's headers. Everything else is private.
5. **When not to.** Don't add modules "just in case": every dependency costs build time and couples modules. (This project makes one deliberate, documented exception, item 8.)
6. **Common mistakes.**
   - Making everything public, which spreads include paths and widens rebuilds.
   - A missing dependency: `cannot open include file` or `unresolved external symbol`.
   - A module that depends on another **plugin's** module without the plugin being listed in the descriptor: UBT warns (§5).
   - A circular dependency between two modules (UBT reports it).
7. **Example.**
   ```csharp
   PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
   PrivateDependencyModuleNames.Add("NavReadyPCGCore");
   ```
8. **In this project.**
   - The game module: public `Core`, `CoreUObject`, `Engine`; private `NavReadyPCGCore`.
   - The plugin: public `Core`, `CoreUObject`, `Engine`; private **`PCG` and `NavigationSystem`, declared before any code uses them**, to prove in Phase 4 that the launcher engine links both into the editor and the packaged game. F-1 and F-2 built with 0 warnings; F-5/F-6 show both systems' console variables in the packaged build (`09`).

## 3. `TargetRules`: `Type`, `DefaultBuildSettings`, `IncludeOrderVersion`

1. **What it is.** The C# class in `<Name>.Target.cs` that describes one executable: game, editor, client, server or program.
2. **Why it exists.** The same modules build into different executables, each with different settings and module sets.
3. **How it works.** UBT finds the class `<Name>Target` and builds its `ExtraModuleNames`, the engine and the enabled plugins. `DefaultBuildSettings` selects a versioned set of default build options; `IncludeOrderVersion` selects which engine version's header include order to use.
4. **When to use it.** One file per executable type. Here: `NavReadyPCG` (game) and `NavReadyPCGEditor` (editor).
5. **When not to.** No `Client`/`Server` targets for a single-player project.
6. **Common mistakes.**
   - The class name doesn't follow `<Name>Target`.
   - Leaving an old `IncludeOrderVersion` after an engine upgrade, which hides missing-include bugs.
   - Expecting the **Test** configuration to build on a launcher engine. It doesn't ("Targets cannot be built in the Test configuration with this engine distribution", found by UE5-UpdateBudget).
7. **Example.** `Source\NavReadyPCG.Target.cs` (`05` A2).
8. **In this project.** `V7` and `Unreal5_8`, the values Epic's 5.8 `TP_Blank` template uses. Headline builds are packaged **Development**.

## 4. Descriptor `LoadingPhase`

1. **What it is.** A field on each module in `.uproject`/`.uplugin` that says when the module loads during engine start-up.
2. **Why it exists.** Some modules must load before others: before config is read, before the first world, before the editor UI.
3. **How it works.** The engine loads modules phase by phase: `EarliestPossible`, `PostConfigInit`, `PreLoadingScreen`, `PreDefault`, `Default`, `PostDefault`, `PostEngineInit`, … (`Runtime/Projects/Public/ModuleDescriptor.h:24–55`). A module that another loaded module depends on is loaded first regardless.
4. **When to use which.** `Default` for ordinary gameplay code. Earlier phases only when something must exist before engine systems start (PCG's own `PCGCompute` module loads at `PostConfigInit` for shader registration: `Plugins/PCG/PCG.uplugin`).
5. **When not to.** Don't pick an early phase "to be safe": early modules can't use systems that aren't up yet.
6. **Common mistakes.** A module that loads too late (`PostDefault` or later) whose classes a map or config needs ("class not found" while loading).
7. **Example.** `{ "Name": "NavReadyPCGCore", "Type": "Runtime", "LoadingPhase": "Default" }`
8. **In this project.** Both modules use `Default`. The smoke log shows the plugin mounted and its DLL loaded before `LoadMap`, which is what world subsystems and component classes will need (`09` F-3).

## 5. Plugin dependencies in descriptors

1. **What it is.** The `"Plugins": [ { "Name": …, "Enabled": true } ]` list in a `.uplugin` (plugins this plugin needs; `FPluginDescriptor::Plugins`, `Runtime/Projects/Public/PluginDescriptor.h:173–174`) or a `.uproject` (plugins the project enables).
2. **Why it exists.** A module in plugin A that links against a module in plugin B only works if B is enabled. Declaring it makes the engine enable B whenever A is enabled, and lets UBT check the dependency.
3. **How it works.** UBT compares module dependencies with descriptor dependencies and warns on a mismatch: "Plugin '…' does not list plugin '…' as a dependency, but module '…' depends on module '…'" (`Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:1578`), and the same for a project's modules against its `.uproject` (`:1687`).
4. **When to use it.** Whenever a `Build.cs` lists a module that lives in another plugin.
5. **When not to.** Engine **modules** (such as `NavigationSystem`) are not plugins and never go in this list.
6. **Common mistakes.** Forgetting the entry (a warning today, a missing-module error in a project that has that plugin disabled); listing an engine module as if it were a plugin.
7. **Example.** `"Plugins": [ { "Name": "PCG", "Enabled": true } ]` in `NavReadyPCGCore.uplugin`.
8. **In this project.** `NavReadyPCGCore.uplugin` lists `PCG`, because its module depends on the `PCG` module. The `.uproject` also lists `PCG` (and the project's own plugin), because the game module will depend on PCG from S1. The builds show no dependency warning (`09` F-1, F-2). The same list can also **switch off** a plugin that is enabled by default: the `.uproject` sets `{ "Name": "AndroidFileServer", "Enabled": false }`, because that plugin's editor module writes a random security token into `DefaultEngine.ini` (`05` A1).

## 6. `-ExecCmds=` and reading console variables from the log

1. **What it is.** A command-line argument that queues console commands to run once the engine has started (`Runtime/Engine/Private/UnrealEngine.cpp:2552` queues them; parsing in `Runtime/Engine/Private/ParseExecCommands.cpp:11–90`).
2. **Why it exists.** Automation: set console variables, start captures or quit, without typing in the console.
3. **How it works.** The value is split on commas that are not inside single quotes (`ParseExecCommands.cpp:11–30`), and each command is added to `GEngine->DeferredCommands` (`:69–75`), which the engine runs at the start of a later frame. A command that is just a console variable's name prints its current value and who set it, e.g. `pcg.FrameTime = "5"      LastSetBy: Constructor`.
4. **When to use it.** Smoke tests and benchmark set-up.
5. **When not to.** Not for cheat variables in builds that disallow cheats, and not as a substitute for configuration that must always apply (that belongs in `.ini` files).
6. **Common mistakes.** Forgetting that commands run **deferred** (not before the first frame); separating commands with spaces instead of commas; unquoted values containing commas.
7. **Example.** `-ExecCmds="pcg.FrameTime,quit"`
8. **In this project.** The smoke scripts print `pcg.FrameTime`, `pcg.RuntimeGeneration.NumGeneratingComponents` and `ai.navigation.MaxDirtyAreasCountWhileSuspended`, then `quit`. "LastSetBy: Constructor" confirms each value is the engine default in this build (5, 16 and 32768; the same values the source declares, `01b` §2–3).
