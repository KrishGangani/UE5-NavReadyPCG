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

---

## APIs added in step S1 (the world spike)

| # | API | Where |
|---|---|---|
| 7 | `UCLASS` / `UPROPERTY` / `GENERATED_BODY`, `TObjectPtr` | every S1 class |
| 8 | `UWorldSubsystem` / `UTickableWorldSubsystem` | `UNRBenchmarkSubsystem` |
| 9 | `SpawnActor`, `SpawnActorDeferred` + `FinishSpawning`, `CreateDefaultSubobject`, `NewObject` + `RegisterComponent` | director, cell actor |
| 10 | `UPCGComponent` for runtime generation | `ANRCellActor` |
| 11 | `UPCGGraph` built in code: `AddNodeOfType`, `AddEdge`, `IsInputPinConnected`; Volume Sampler; Static Mesh Spawner | `NRGraphFactory` |
| 12 | `APCGWorldActor` spawned at runtime | director |
| 13 | Navigation at runtime: `ANavMeshBoundsVolume`, `OnNavigationBoundsUpdated`, `RuntimeGeneration`, `CrowdManagerClass` | director, config |
| 14 | Navigation queries: `FindPathSync`, `FPathFindingQuery`, `ProjectPointToNavigation`, `IsNavigationBuildInProgress`, `HasDirtyAreasQueued` | director |
| 15 | Reflection writes: `FindPropertyByName`, `CastField<FStructProperty>`, `ContainerPtrToValuePtr` | `ANRCellActor` |
| 16 | Small utilities: `FParse`, `LoadObject`, logging, `RequestExit` / `RequestExitWithStatus`, `ComponentHasTag`, `SetViewTarget` | director |

## 7. `UCLASS` / `UPROPERTY` / `GENERATED_BODY`, `TObjectPtr`

1. **What it is.** Macros that register a class and its members with Unreal's reflection system; Unreal Header Tool (UHT) reads them and generates code (`*.generated.h`, `*.gen.cpp`). `TObjectPtr<T>` is the member pointer type for UObjects in UE5.
2. **Why it exists.** Reflection gives garbage collection, serialization, Blueprint access, the editor's property panels and network replication. GC can only see references that are reflected.
3. **How it works.** `GENERATED_BODY()` expands into the generated declarations (`StaticClass()`, constructors, and so on). A `UPROPERTY` object reference is reported to the garbage collector, which keeps the object alive and nulls the reference if the object is destroyed (at the next GC).
4. **When to use it.** For every UObject member a class owns or must keep alive.
5. **When not to.** Plain data that no system needs to see (counters, timestamps) doesn't need `UPROPERTY`.
6. **Common mistakes.** A raw `UObject*` member without `UPROPERTY` (GC can free the object under you); forgetting that `IsValid()` is still needed between a `Destroy()` and the next GC.
7. **Example.** `UPROPERTY(Transient) TObjectPtr<UPCGGraph> Graph;`
8. **In this project.** The director holds the graph, the prop meshes, the 16 cells and the camera as `UPROPERTY(Transient)` members (transient: never saved). The cell actor's two components are `UPROPERTY(VisibleAnywhere)`, shown read-only in the editor.

## 8. `UWorldSubsystem` / `UTickableWorldSubsystem`

1. **What it is.** A UObject the engine creates automatically for every world, with the world's lifetime (`Runtime/Engine/Public/Subsystems/WorldSubsystem.h`). The tickable variant is also an `FTickableGameObject`.
2. **Why it exists.** World-scoped managers without placing an actor in a map.
3. **How it works.** `Initialize` when the world is created; `OnWorldBeginPlay` once play starts (before actors' `BeginPlay`); `Deinitialize` at teardown. A tickable subsystem's `Tick` runs with the other tickable objects after `TG_PostPhysics` (`LevelTick.cpp:1821`). `DoesSupportWorldType` filters which worlds get one.
4. **When to use it.** One-per-world systems: our director now, NavReady's pipeline from S3.
5. **When not to.** Per-actor state (use a component), or state that must outlive a world (`UGameInstanceSubsystem`).
6. **Common mistakes.** Forgetting that editor worlds get subsystems too; doing world set-up in `Initialize` (actors can't be spawned yet); a tickable that does work when it has nothing to do.
7. **Example.** `bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }`
8. **In this project.** `UNRBenchmarkSubsystem`: Game and PIE worlds only; builds the arena in `OnWorldBeginPlay`; its `Tick` does nothing unless `-NRRun` was given (`-NRSpike` in S1).

## 9. Spawning actors and components at runtime

1. **What it is.** `UWorld::SpawnActor<T>(…)`, `UWorld::SpawnActorDeferred<T>(…)` + `AActor::FinishSpawning(…)` (`World.h`), `CreateDefaultSubobject<T>(Name)` in a constructor, `NewObject<T>(Outer, Name)` + `RegisterComponent()` for a component created later.
2. **Why it exists.** Actors and components need engine-side registration (render, physics, navigation state) that `new` would skip.
3. **How it works.** `SpawnActor` constructs, registers components and runs `BeginPlay`. The deferred version stops before `BeginPlay`/construction scripts until `FinishSpawning`, so properties can be set first. Components register during the spawn itself (`Actor.cpp:4332–4336`).
4. **When to use it.** Deferred spawn whenever a component reads its settings in `BeginPlay`.
5. **When not to.** Don't defer when nothing needs configuring: an unfinished deferred actor is a half-alive object.
6. **Common mistakes.** Forgetting `FinishSpawning` on an error path (fixed in S1 by `Destroy()`); creating a component with `NewObject` and never registering it (it never renders or collides); attaching after registering.
7. **Example.**
   ```cpp
   ANRCellActor* Cell = World.SpawnActorDeferred<ANRCellActor>(ANRCellActor::StaticClass(), Transform);
   Cell->Configure(...);
   Cell->FinishSpawning(Transform);
   ```
8. **In this project.** Cells are spawned deferred (their PCG component registers in `BeginPlay`); the floor, bounds volume, PCG world actor and camera with plain `SpawnActor`; the bounds volume's extent box with `NewObject` → `SetupAttachment` → `RegisterComponent`.

## 10. `UPCGComponent` for runtime generation

1. **What it is.** The component that runs a PCG graph for its actor (`Plugins/PCG/Source/PCG/Public/PCGComponent.h`).
2. **Why it exists.** It owns the graph instance, the seed, the generated resources and the generation state.
3. **How it works.** With `GenerationTrigger = GenerateAtRuntime` (`:77–82`) it registers with the PCG subsystem in `BeginPlay` (`PCGComponent.cpp:1743–1767`), and the runtime-generation scheduler generates it when a generation source is within its radius of its bounds (`PCGRuntimeGenScheduler.cpp:937–983`). Native delegates report start, generated, cancelled and cleaned (`PCGComponent.h:380–383`).
4. **When to use it.** Content that should appear around the player at runtime.
5. **When not to.** Content that can be baked in the editor (`GenerateOnLoad` with saved output is cheaper at runtime).
6. **Common mistakes.** Leaving the radius at the graph default (effectively unlimited: everything generates at once); expecting the scheduler without a PCG world actor (§12); missing the scheduling policy object on a component created in code; writing the protected radius members directly (§15).
7. **Example.** see `ANRCellActor::Configure` (`05` B2).
8. **In this project.** One non-partitioned runtime component per cell; radius 35 m, cleanup scalar 100 (never during a run), seed per cell, `UPCGSchedulingPolicyDistanceAndDirection` created explicitly, graph assigned with `SetGraphLocal`.

## 11. `UPCGGraph` built in code

1. **What it is.** The graph asset type, here created in memory: `UPCGGraph::AddNodeOfType<TSettings>(TSettings*&)`, `AddEdge`, plus `UPCGNode::IsInputPinConnected` (`PCGGraph.h:454, 477`; `PCGNode.h:106`).
2. **Why it exists.** Tools and tests build graphs programmatically; the functions are outside `WITH_EDITOR`, so they work in a packaged game (proved in S1).
3. **How it works.** A new graph already has an input and an output node. `AddNodeOfType` creates a node with a settings object of the given class; `AddEdge` connects an output pin to an input pin by label.
4. **When to use it.** When a graph must be reviewable as text or generated from data.
5. **When not to.** For artist-authored content: an editor asset is easier to iterate on.
6. **Common mistakes.** Using a settings class that isn't exported (link error: Create Points Grid, Create Points, Random Choice); trusting `AddEdge`/`AddLabeledEdge` to report success (`07`, "Using PCG from C++"); using the wrong input-node label (`"In"`, not `"Input"`).
7. **Example.** see `NRGraphFactory::BuildRockFieldGraph` (`05` B1).
8. **In this project.** Input `"In"` → Volume Sampler (`"Volume"`) → Static Mesh Spawner, with one weighted mesh entry: collision `BlockAll`, *Can Ever Affect Navigation* per configuration.

## 12. `APCGWorldActor` at runtime

1. **What it is.** PCG's per-world settings actor (`PCGWorldActor.h:16–17`: `MinimalAPI, NotBlueprintable, NotPlaceable`), normally created by the editor and saved in the level.
2. **Why it exists.** It holds world-wide PCG settings (partition grid size, generation-source options) and is the anchor the runtime scheduler requires.
3. **How it works.** It registers with the PCG subsystem in `BeginPlay` (`PCGWorldActor.cpp:187–191`); the subsystem ticks the scheduler only `if (PCGWorldActor)` (`PCGSubsystem.cpp:425–428`) and never creates one at runtime (`:431–443`).
4. **When to use it.** Spawn one at runtime only when the map has none (a level built by code, as here).
5. **When not to.** In an authored map: the editor creates and saves it.
6. **Common mistakes.** Spawning two (the second merges into the first, `PCGSubsystem.cpp:648–659`); expecting runtime generation without one.
7. **Example.** `World.SpawnActor<APCGWorldActor>(APCGWorldActor::StaticClass(), FTransform::Identity, SpawnParams);`
8. **In this project.** Spawned once in `BuildArena`. **Measured in S1:** with it, the scheduler generated the test cell when the camera came within 35 m, in a packaged build.

## 13. Navigation at runtime

1. **What it is.** `ANavMeshBoundsVolume` (`NavigationSystem/Public/NavMesh/NavMeshBoundsVolume.h:16–17`, `MinimalAPI`), `UNavigationSystemV1::OnNavigationBoundsUpdated(ANavMeshBoundsVolume*)` (public, `BlueprintCallable`), and two config properties: `ANavigationData::RuntimeGeneration` and `UNavigationSystemV1::CrowdManagerClass`.
2. **Why it exists.** Navigation is built inside bounds volumes; runtime levels need runtime bounds.
3. **How it works.** A volume reports its area as its components' bounding box (`NavigationSystem.cpp:4400–4413`). When bounds appear and there is no navigation data, the engine spawns it (`:4375–4390`) and builds it if its `RuntimeGeneration` isn't `Static`.
4. **When to use it.** Levels generated or assembled at runtime.
5. **When not to.** Authored levels, where volumes and navmesh are built and saved in the editor (cheaper: no runtime build).
6. **Common mistakes.** Spawning a volume and expecting the brush to have a size (it has none at runtime); leaving `RuntimeGeneration=Static` (the engine warns and builds once, and never again); keeping the crowd manager without crowd agents (a warning at every exit in this set-up).
7. **Example.** see `BuildArena` (`05` B3).
8. **In this project.** A runtime volume with a 200 m × 200 m × 10 m box; `RuntimeGeneration=Dynamic`; no crowd manager. **Measured in S1:** a complete 268.7 m path corner to corner within 0.06–0.35 s of start.

## 14. Navigation queries

1. **What it is.** `UNavigationSystemV1::FindPathSync(FPathFindingQuery, …)` (`NavigationSystem.h:658`) → `FPathFindingResult` (`NavigationData.h:57–70`); `ProjectPointToNavigation(Point, Out, Extent, NavData)` (`NavigationSystem.h:718`); `IsNavigationBuildInProgress()`, `HasDirtyAreasQueued()`, `GetDefaultNavDataInstance(DontCreate)`.
2. **Why it exists.** Ask the navmesh for a path, the nearest navigable point, and whether it is busy.
3. **How it works.** `FPathFindingQuery(Owner, NavData, Start, End)` (`NavigationSystemTypes.h:60`) runs an A* search on the Recast navmesh on the calling thread. `ProjectPointToNavigation` finds navmesh within a box around a point.
4. **When to use it.** Synchronous queries are fine for checks and small numbers of agents; S4's agents use `FindPathSync` at a fixed rate.
5. **When not to.** Hundreds of queries per frame: use `FindPathAsync`.
6. **Common mistakes.** Treating `IsSuccessful()` as "complete" (check `IsPartial()` too); a projection extent so tall it finds navmesh on top of an obstacle (fixed in S1); `GetDefaultNavDataInstance()` with the creating overload in a query path.
7. **Example.** `const FPathFindingResult R = NavSys->FindPathSync(FPathFindingQuery(this, *NavData, Start, End));`
8. **In this project.** S1: the spike's navmesh and carve checks. S2: the warm-up path, the pocket check and the end-of-run idle wait. The readiness probe (S3) and agents (S4) build on the same calls.

## 15. Writing a reflected property: `FindPropertyByName`, `CastField`, `ContainerPtrToValuePtr`

1. **What it is.** Reflection access to a `UPROPERTY` by name: `UStruct::FindPropertyByName`, `CastField<FStructProperty>`, `FProperty::ContainerPtrToValuePtr<T>(Container)` (`CoreUObject/Public/UObject/UnrealType.h`).
2. **Why it exists.** It is how Blueprint, serialization and the editor read and write properties without compile-time access.
3. **How it works.** A property object knows its offset inside its owning struct or class; `ContainerPtrToValuePtr` adds that offset to the container's address.
4. **When to use it.** Only when a property is reflected but not reachable from C++ (protected, as `FPCGRuntimeGenerationRadii`'s radii are).
5. **When not to.** Whenever a public member or setter exists: reflection by name fails only at run time, not at compile time.
6. **Common mistakes.** Not checking the property's type before writing through the pointer; assuming a name survives engine upgrades (check and log, as `SetRadiiMember` does); forgetting a follow-up the setter would have done (`ComputeHash()` here).
7. **Example.** see `NRCell::SetRadiiMember` (`05` B2).
8. **In this project.** Two writes per cell, at spawn. The struct type is compared by name because `FPerQualityLevelFloat::StaticStruct()` isn't exported.

## 16. Small utilities used in S1

| API | Use here | Note |
|---|---|---|
| `FCommandLine::Get()`, `FParse::Param`, `FParse::Value` | `-NRRun`, `-NRSeed=`, `-NRRunTag=`, `-NRTimeout=`, `-NRExit` (S1: `-NRSpike`) | `Param` matches `-Name`; `Value` reads after `Name=` |
| `LoadObject<UStaticMesh>(nullptr, Path)` | the cube and the cylinder, once, at begin play | synchronous; the folder must be cooked (`DirectoriesToAlwaysCook`) |
| `DEFINE_LOG_CATEGORY_STATIC`, `UE_LOG` | `LogNRBench`, `LogNRGraph`, `LogNRCell` | file-local categories; `Error` lines make failures visible in the queue log |
| `FPlatformMisc::RequestExit(false)` / `RequestExitWithStatus(true, 1)` | exit after the checks | on Windows only the forced form returns a non-zero code (`WindowsPlatformMisc.cpp:1493–1521`) |
| `UActorComponent::ComponentHasTag`, `AActor::GetComponents` | finding PCG-generated components | PCG tags them `"PCG Generated Component"` (`PCGHelpers.h:35`) |
| `ACameraActor`, `APlayerController::SetViewTarget` | the generation source | PCG's player source reads the view point |
| `UCollisionProfile::NoCollision_ProfileName` / `BlockAll_ProfileName` | volumes vs props | `BlockAll` blocks Pawn, so it affects navigation |
| `AStaticMeshActor::SetMobility(Movable)` | the floor | Static components refuse `SetStaticMesh` after begin play |
| `FBox::ComputeSquaredDistanceToPoint` | camera-to-cell distance (S1 spike only; removed in S2) | the scheduler's own measure |
| Delegate `AddUObject(this, &Class::Method)` | PCG start/generated events | weak binding: skipped if the object is gone |

---

## APIs added in step S2 (content and the determinism hash)

| # | API | Where |
|---|---|---|
| 17 | `FCrc`: `MemCrc32`, `StrCrc32Len`, `TypeCrc32` | `NavReadyContentHash`, director |
| 18 | Automation tests: `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `RunTest`, `TestEqual` / `TestNotEqual` / `TestTrue`, test flags, running them headless | plugin tests, `run_tests.bat` |
| 19 | PCG Transform Points and the weighted mesh selector; PCG's seeds | `NRGraphFactory` |
| 20 | Reading instances: `UInstancedStaticMeshComponent::GetInstanceCount`, `GetInstanceTransform` | `NavReadyContentHash::HashComponents` |
| 21 | Navmesh tiles and idleness: `ARecastNavMesh::GetTileSizeUU`, `NavMeshOriginOffset`, `IsNavigationBuildInProgress`, `HasDirtyAreasQueued` | director |
| 22 | Delegates with a payload: `AddUObject(this, &Method, Payload)` | director |
| 23 | Small utilities used in S2 | director, hash |

## 17. `FCrc`: `MemCrc32`, `StrCrc32Len`, `TypeCrc32`

1. **What it is.** Static CRC-32 functions over memory, strings and single values (`Runtime/Core/Public/Misc/Crc.h:29, 36, 43, 65`).
2. **Why it exists.** Cheap, stable checksums: the same bytes always give the same value, on every run and machine.
3. **How it works.** Each takes an optional starting value, so CRCs can be chained: `MemCrc32(Data, Bytes, Seed)`. `StrCrc32(Data, CRC)` reads up to a null terminator, and its second parameter is a **seed**; `StrCrc32Len(Data, Length, CRC)` reads exactly `Length` characters. `TypeCrc32(Value, CRC)` hashes one value's bytes.
4. **When to use it.** Determinism checks and change detection over plain data you control. Epic's own comment prefers `Hash/Fnv.h` for new code ("CRCs are not very good and fairly slow"); for a few thousand values per cell, once per generation, speed doesn't matter, and CRC-32 is the one every reader knows.
5. **When not to.** Security (CRC is trivially forgeable), hash tables (use `GetTypeHash`), or any memory with padding bytes or pointers: their values aren't part of the content.
6. **Common mistakes.** `StrCrc32(View.GetData(), View.Len())`: compiles, but the length becomes the seed and the read runs past the view to the next null. Hashing an `FName`'s index or an object pointer (both change between runs). Hashing a struct with padding.
7. **Example.** `FCrc::MemCrc32(Values, sizeof(Values), FCrc::StrCrc32Len(Path.GetData(), Path.Len()))`
8. **In this project.** One CRC per instance (quantized transform seeded with the mesh path's CRC), one over the sorted instance CRCs per cell, and `TypeCrc32` to chain the 16 cell hashes into one run hash.

## 18. Automation tests

1. **What it is.** Unreal's test framework (`Runtime/Core/Public/Misc/AutomationTest.h`). `IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, "Dotted.Name", Flags)` declares a test class (`:4297`); you write its `RunTest(const FString& Parameters)`.
2. **Why it exists.** Tests that run inside the engine, from the editor's Session Frontend or from the command line, with their results in the log and a report.
3. **How it works.** Each `Test…` call records an error on failure and returns `bool`; `RunTest` returns whether the test passed. **Flags:** at least one context (`EditorContext`, `ClientContext`, …: where the test may run) and **exactly one** filter (`ProductFilter`, `EngineFilter`, …), enforced by a `static_assert` in the macro (`:4127–4134`). From the command line, `-ExecCmds="Automation RunTests <Filter>;Quit"` runs every test whose name starts with `<Filter>`. The automation command splits its own text at `;` (`Developer/AutomationController/Private/AutomationCommandline.cpp:582`), and its `Quit` exits with −1 if any test failed (`:491–504`).
4. **When to use it.** Pure logic with no world: hashing, the readiness policy (S5).
5. **When not to.** Behaviour that needs a running packaged game: that is what the run checks (`s2_checks.bat`) are for.
6. **Common mistakes.** Two filter flags (compile error) or none. `TestEqual` with `uint32`: there are overloads for `int32`, `int64`, `SIZE_T`, `float` and `double` (`:1985–1989`), so the call is ambiguous; cast to `int64`. `TestNotEqual` is a template (`:2412`) and takes any comparable type.
7. **Example.** `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavReadyHashEmptyTest, "NavReadyPCG.Hash.Empty", NavReadyTests::Flags)`
8. **In this project.** Five hash tests in the plugin under `NavReadyPCG.Hash.*`, run by `Tools\run_tests.bat`.

## 19. PCG Transform Points, the weighted mesh selector, and PCG's seeds

1. **What it is.** `UPCGTransformPointsSettings` (`MinimalAPI`, so creatable from our code: `PCG/Public/Elements/PCGTransformPoints.h:12–13`) offsets, rotates and scales each point by random amounts within ranges (`OffsetMin/Max`, `RotationMin/Max`, `ScaleMin/Max`, `bUniformScale`: `:48–79`). The spawner's weighted mesh selector picks one mesh entry per point.
2. **Why it exists.** Natural-looking variation from one regular point set.
3. **How it works.** Randomness in PCG is seeded, never global. The Volume Sampler gives each point a seed from its lattice indices (`PCGVolumeSampler.cpp:157`). A node that uses seeds combines its own seed with the component's (`UPCGSettings::GetSeed`, `PCGSettings.cpp:723–726`); Transform Points then draws from `FRandomStream(ComputeSeed(NodeAndComponentSeed, PointSeed))` (`PCGTransformPoints.cpp:70, 170`), and the weighted selector from a stream seeded with the point, node and component seeds (`PCGMeshSelectorWeighted.cpp:228–230`, `PCGHelpers.cpp:55–61`).
4. **When to use it.** Whenever content should vary per instance but be reproducible.
5. **When not to.** For a layout that must stay exactly regular.
6. **Common mistakes.** Expecting different content from cells with the same component seed: the point seeds come from world lattice positions, so only the component seed (or position) tells cells apart. Forgetting `bUniformScale` reads only the X components of the scale range.
7. **Example.** `Transform->RotationMax = FRotator(0.0, 360.0, 0.0);` (yaw only)
8. **In this project.** Jitter ±25 cm in X and Y, any yaw, uniform scale 0.9–1.4; cube and cylinder at weight 1. Cell seed = base seed × 100 + index.

## 20. Reading instances: `GetInstanceCount`, `GetInstanceTransform`

1. **What it is.** `UInstancedStaticMeshComponent::GetInstanceCount()` and `GetInstanceTransform(Index, OutTransform, bWorldSpace = false)` (`Runtime/Engine/Classes/Components/InstancedStaticMeshComponent.h:435, 338`).
2. **Why it exists.** Read access to the per-instance data of an ISM.
3. **How it works.** Instance transforms are stored relative to the component; with `bWorldSpace = true` the component's transform is applied. Returns `false` for an invalid index.
4. **When to use it.** Inspecting or hashing generated content.
5. **When not to.** Every frame over large ISMs (it copies a transform per call).
6. **Common mistakes.** Forgetting `bWorldSpace = true`: the hash would then miss a component placed differently.
7. **Example.** `ISM->GetInstanceTransform(Index, Transform, /*bWorldSpace=*/true)`
8. **In this project.** `NavReadyContentHash::HashComponents`, once per cell when PCG reports it generated.

## 21. Navmesh tiles and idleness

1. **What it is.** `ARecastNavMesh::GetTileSizeUU()` (the real tile side: `TileSizeUU` truncated to whole cells, `NavigationSystem/Private/NavMesh/RecastNavMesh.cpp:1894–1899`); `ARecastNavMesh::NavMeshOriginOffset` (where the tile grid starts, default the world origin: `Public/NavMesh/RecastNavMesh.h:788–790`); `UNavigationSystemV1::IsNavigationBuildInProgress()` and `HasDirtyAreasQueued()`.
2. **Why it exists.** Tile layout for streaming and tools; a global "is navigation busy" signal.
3. **How it works.** `IsNavigationBuildInProgress` asks each navigation data's generator; Recast's answers true while tiles are running or pending (`RecastNavMeshGenerator.cpp:7764–7769`, `NavigationSystem.cpp:4889–4911`). `HasDirtyAreasQueued` is true while areas wait to be turned into dirty tiles (`NavigationSystem.cpp:3561–3564`). Both together mean "nothing queued, nothing building" for the whole world.
4. **When to use it.** Layout checks; waiting for a settled world (end of a run, a test).
5. **When not to.** Per-region readiness: both are global, so one busy region hides every finished one (`07`). The per-area check is `HasDirtyTiles(FBox)` (S3).
6. **Common mistakes.** Assuming `TileSizeUU` is the tile size (1000 gives 988 cm with 19 cm cells). Treating global idleness as "this region is ready".
7. **Example.** `FMath::IsNearlyEqual(NavMesh->GetTileSizeUU(), 988.0, 0.01)`
8. **In this project.** The warm-up checks the tiles match the cells; the end of a run waits for idleness before the pocket check. `IsNavigationBuildInProgress` isn't `const`, so the helper that calls it gets the navigation system through `UWorld*`.

## 22. Delegates with a payload

1. **What it is.** Binding extra values to a delegate at bind time: `Delegate.AddUObject(this, &UMyClass::Handler, Payload…)` (`Runtime/Core/Public/Delegates/DelegateSignatureImpl.inl:1020`).
2. **Why it exists.** One handler for many senders, each identified without a search.
3. **How it works.** The handler receives the delegate's own parameters first, then the bound values: for `FOnPCGGraphGenerated` (`DECLARE_MULTICAST_DELEGATE_OneParam(…, UPCGComponent*)`, `PCG/Public/PCGComponent.h:57`), `void Handler(UPCGComponent*, int32 CellIndex)`. `AddUObject` holds the object weakly (`TBaseUObjectMethodDelegateInstance` stores a `TWeakObjectPtr`, `Delegates/DelegateInstancesImpl.h:511, 635`): the call is skipped if it has been destroyed.
4. **When to use it.** Per-sender context that is known at bind time.
5. **When not to.** Values that change after binding (they are copied at bind time).
6. **Common mistakes.** Payload parameters placed before the delegate's own; binding a reference payload (it is copied, `std::decay_t`).
7. **Example.** `Cell->GetPCG()->OnPCGGraphGeneratedDelegate.AddUObject(this, &UNRBenchmarkSubsystem::HandleCellGenerated, Index);`
8. **In this project.** The director's two PCG handlers, bound per cell with its index.

## 23. Small utilities used in S2

| API | Use here | Note |
|---|---|---|
| `FFileHelper::SaveStringToFile`, `FPaths::ProjectSavedDir`, `FPaths::Combine` | the run report `Saved/NRRuns/<tag>.csv` | `Misc/FileHelper.h:196`, `Misc/Paths.h:290`; the writer creates a missing folder (`HAL/FileManagerGeneric.cpp:112–117`) |
| `FPlatformTime::Seconds()` | generation times | wall-clock seconds (`Windows/WindowsPlatformTime.h:23`) |
| `Algo::CountIf` | cells generated | `Algo/Count.h:36` |
| `TArray::StableSort(Predicate)` | components by mesh path | equal keys keep their order (`Containers/Array.h:3704`) |
| `TConstArrayView<T>`, `FStringView` | hash inputs | non-owning views: no copy; a string view isn't null-terminated |
| `FMath::RoundToDouble`, `FMath::Fmod` | quantization; the lattice check | |

---

## APIs added in step S3 (the readiness observer and the harness)

| # | API | Where |
|---|---|---|
| 24 | `UENUM(BlueprintType)` | `ENavReadyCellState` |
| 25 | `UActorComponent` lifetime: `BeginPlay` / `EndPlay`, `FindComponentByClass`, `UWorld::GetSubsystem` | `UNavReadyCellComponent` |
| 26 | Dynamic multicast delegates and `BlueprintAssignable` | `OnCellStateChanged` |
| 27 | The readiness probe: `IsNavigationBuildingLocked`, `HasDirtyAreasQueued`, `FRecastNavMeshGenerator::HasDirtyTiles(FBox)`, `GrowBoundingBox` | `UNavReadySubsystem` |
| 28 | The CSV profiler: categories, exclusive timing, custom stats, events, metadata, capture by event | subsystem, director, `run_bench.py` |
| 29 | Small utilities used in S3 | |

## 24. `UENUM(BlueprintType)`

1. **What it is.** Registers an enum with reflection (`*.generated.h`), so Blueprint, the editor and dynamic delegates can use it.
2. **Why it exists.** Plain C++ enums are invisible to Blueprint and can't be a `UPROPERTY` or a dynamic delegate parameter.
3. **How it works.** UHT generates a `UEnum` object and `StaticEnum<T>()`. `BlueprintType` enums must be `uint8`.
4. **When to use it.** A state shared with Blueprint, the editor or reflected delegates.
5. **When not to.** Purely internal states (the director's `EPhase` stays a plain `enum class`).
6. **Common mistakes.** A `BlueprintType` enum that isn't `uint8`; forgetting the header then needs its `.generated.h` include as the last include.
7. **Example.** `UENUM(BlueprintType) enum class ENavReadyCellState : uint8 { Idle, … };`
8. **In this project.** The cell's state, broadcast to Blueprint by `OnCellStateChanged`.

## 25. Component lifetime and finding siblings

1. **What it is.** `UActorComponent::BeginPlay` / `EndPlay`, `AActor::FindComponentByClass<T>()` (`Runtime/Engine/Classes/GameFramework/Actor.h:3826`), `UWorld::GetSubsystem<T>()` (`Engine/World.h:4312`).
2. **Why it exists.** A component starts working once its world plays and must let go before it is destroyed.
3. **How it works.** `BeginPlay` runs when the owner begins play (after all its components are registered); `EndPlay` on destruction, level unload or world teardown. `FindComponentByClass` returns the first component of that class on the owner. `GetSubsystem` returns the world's instance, or `nullptr` if the subsystem doesn't exist for this world type.
4. **When to use it.** Binding to sibling components and to world services.
5. **When not to.** In the constructor: siblings and the world aren't there yet.
6. **Common mistakes.** Binding in `BeginPlay` and never unbinding (a dangling `AddUObject` is skipped once the object is gone, but stays in the list); assuming a subsystem exists in editor worlds.
7. **Example.** `PCG = GetOwner()->FindComponentByClass<UPCGComponent>();`
8. **In this project.** `UNavReadyCellComponent` binds the PCG component's four delegates and registers with `UNavReadySubsystem` in `BeginPlay`, and undoes both in `EndPlay`.

## 26. Dynamic multicast delegates and `BlueprintAssignable`

1. **What it is.** `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(Type, P1Type, P1Name, P2Type, P2Name)` (`Runtime/Core/Public/Delegates/DelegateCombinations.h:62`): a reflected event; `UPROPERTY(BlueprintAssignable)` lets Blueprint bind to it.
2. **Why it exists.** Native delegates (`DECLARE_MULTICAST_DELEGATE…`) are faster but invisible to Blueprint.
3. **How it works.** Bindings are by function name through reflection, so parameters must be reflected types, and the names in the macro become the Blueprint pin names.
4. **When to use it.** Events a designer or Blueprint should react to.
5. **When not to.** Hot paths and C++-only listeners: `UNavReadySubsystem::OnAnyCellStateChanged` is native.
6. **Common mistakes.** A non-reflected parameter type; broadcasting while iterating a container the listener can change.
7. **Example.** `Component->OnCellStateChanged.Broadcast(Component, NewState);`
8. **In this project.** One per cell (Blueprint), one native for all cells (the director, the baselines in S4).

## 27. The readiness probe

1. **What it is.** `UNavigationSystemV1::IsNavigationBuildingLocked(Flags = ~0)` (inline, `NavigationSystem.h:1083`), `HasDirtyAreasQueued()` (`NavigationSystem.cpp:3561–3564`), and on the Recast generator `HasDirtyTiles(const FBox&)` and `GrowBoundingBox(const FBox&, bool)` (exported, `NavMesh/RecastNavMeshGenerator.h:830, 836`).
2. **Why it exists.** Navigation's own view of what is still being built, globally and per area.
3. **How it works.** `HasDirtyTiles(Box)` converts the box to a tile range (`FRcTileBox`: a max edge exactly on a tile boundary counts the lower tile, `.h:240–257`) and looks for pending or running tile builds in it (`RecastNavMeshGenerator.cpp:6644–6670`). `GrowBoundingBox` adds the navmesh's border (`:6596–6601`): a change near a tile edge also dirties the next tile, because the engine grows every dirty area the same way first (`GrowDirtyBounds`, `:6603–6608`, used at `:6787`).
4. **When to use it.** Deciding when one region's navigation is current.
5. **When not to.** With a non-Recast navigation data: cast to `ARecastNavMesh` first; only then is the generator a `FRecastNavMeshGenerator`.
6. **Common mistakes.** Caching the generator (a full rebuild can recreate it); checking the content's own bounds instead of the grown ones; treating the global checks as per-region.
7. **Example.** `const FBox Grown = Generator->GrowBoundingBox(Content, false); const FVector G = FVector::Max(Content.Min - Grown.Min, Grown.Max - Content.Max); Generator->HasDirtyTiles(FBox(Content.Min - G, Content.Max + G));` (the larger growth on both sides, because the engine's dirty-area growth swaps low and high)
8. **In this project.** `UNavReadySubsystem::Tick`, every frame, for every committed or ready cell.

## 28. The CSV profiler

1. **What it is.** Unreal's per-frame CSV capture (`Runtime/Core/Public/ProfilingDebugging/CsvProfiler.h`): `CSV_DEFINE_CATEGORY(Name, bOnByDefault)` (`:50`), `CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Name)` (`:129`), `CSV_CUSTOM_STAT(Category, Name, Value, Op)` (`:155`), `CSV_EVENT(Category, Format, …)` (`:56`), `CSV_METADATA(Key, Value)` (`:85`).
2. **Why it exists.** Frame-by-frame numbers for whole runs, with little overhead, in packaged Development builds.
3. **How it works.** One row per frame. An exclusive timing stat becomes `Exclusive/GameThread/<Name>` and is subtracted from any enclosing exclusive stat; the engine's own include `NavigationBuild` (`Engine/Private/LevelTick.cpp:1645`) and `Tickables` (`Engine/Private/Tickable.cpp:173`). Custom stats become `<Category>/<Name>` columns; navigation records `NavTasks/NumRemainingTasks_<navigation data>` every navigation tick (`NavigationSystem.cpp:1796–1806`). Events land in the `EVENTS` column of their frame. `-csvStartOnEvent=` / `-csvStopOnEvent=` start and stop the capture on an event whose **text** matches, case-insensitively, without its category (`Core/Private/ProfilingDebugging/CsvProfiler.cpp:4759, 4769, 5080–5090`); the start event itself is not in the capture. Metadata goes into the capture's last row.
4. **When to use it.** Benchmarks: frame-time distributions, cost splits, timelines.
5. **When not to.** Shipping builds (compiled out); deep call trees (use Unreal Insights).
6. **Common mistakes.** Expecting the start event in the capture; a category defined twice; reading a capture that wasn't finalised (no end header and metadata row: the run crashed).
7. **Example.** `CSV_CUSTOM_STAT(NavReady, CellsReady, Count, ECsvCustomStatOp::Set);`
8. **In this project.** Category `NavReady` (cost, counts, `CellCommitted_<i>` / `CellReady_<i>` events) and `NRBench` (`BenchStart`, `BenchEnd`); `run_bench.py` passes `-csvStartOnEvent=BenchStart -csvStopOnEvent=BenchEnd`.

## 29. Small utilities used in S3

| API | Use here | Note |
|---|---|---|
| `GFrameCounter` | commit and probe frames | `uint64`, the engine's frame number (`Core/Public/CoreGlobals.h:532`) |
| `AActor::GetComponentsBoundingBox(bNonColliding = true)` | a cell's content bounds | `GameFramework/Actor.h:3585`; `true` includes the non-colliding cell box |
| `TWeakObjectPtr<T>` | queued broadcasts | `Get()` returns `nullptr` once the object is gone |
| `TArray::RemoveAtSwap` | unregistering a cell | O(1): the last element fills the gap, so order isn't kept |
| `TArray::IndexOfByKey` | finding a cell | linear: fine for tens of cells (`12`) |

---

## APIs added in the final step

## 30. `SetCanEverAffectNavigation` on a live ISM, and `AddDirtyArea`

1. **What it is.** `UActorComponent::SetCanEverAffectNavigation(bool)` (`Engine/Private/Components/ActorComponent.cpp:3266–3274`) and `UNavigationSystemV1::AddDirtyArea(const FBox&, ENavigationDirtyFlag, FName)` (`NavigationSystem/Public/NavigationSystem.h:973`; flags in `Engine/Classes/AI/Navigation/NavigationDirtyArea.h:12–21`).
2. **Why it exists.** To make a component navigation-relevant after it was created, and to tell navigation an area changed.
3. **How it works.** Switching the flag on a registered component registers it with the navigation octree (`ActorComponent.cpp:3276–3288`). But an instanced static mesh opts out of the dirty area on that registration, because it pushes per-instance updates instead (`InstancedStaticMesh.cpp:5513–5519`, `NavigationDataHandler.cpp:50`); so an ISM whose instances already exist is registered without its tiles being rebuilt. `AddDirtyArea` marks the area explicitly.
4. **When to use it.** Handing content that already exists to navigation in one step: NavReady's commit.
5. **When not to.** Per instance: adding instances to a navigation-relevant ISM already dirties their areas.
6. **Common mistakes.** Assuming the flag switch alone rebuilds the navmesh (it didn't, in this project's final batch: `09`).
7. **Example.** `ISM->SetCanEverAffectNavigation(true); NavSys->AddDirtyArea(ISM->Bounds.GetBox(), ENavigationDirtyFlag::All, TEXT("NavReadyCommit"));`
8. **In this project.** `UNavReadySubsystem::Tick`, the commit.
