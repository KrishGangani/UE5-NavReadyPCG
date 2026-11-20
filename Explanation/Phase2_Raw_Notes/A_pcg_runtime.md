> **Raw Phase 2 research notes (A_pcg_runtime), UE 5.8.4, 2026-11-21.** Kept as an evidence trail. Every claim cites a `File:line` or a URL. **The claims the design depends on were re-checked by hand and are restated in [../01b_Research_Findings.md](../01b_Research_Findings.md). Treat anything here that is not restated there as unverified.**

# P08 NavReadyPCG: Phase 2 research, part A: PCG runtime generation (UE 5.8.4)

- Engine: `C:\Program Files\Epic Games\UE_5.8\Engine`. `Build/Build.version` reads `"MajorVersion": 5, "MinorVersion": 8, "PatchVersion": 4, "Changelist": 58902052`.
- Paths below are relative to `Engine/`. `PCG/` means `Engine/Plugins/PCG/Source/PCG/`.
- **[inferred]** marks conclusions drawn from following the code flow, not from a direct statement in the source.
- Read-only research. No engine file was modified.

---

## A1. Does PCG talk to the navigation system at all?

| Fact | Source |
|---|---|
| `PCG.Build.cs` has **no** `NavigationSystem` or `AIModule` dependency (public: ComputeFramework, Core, CoreUObject, DeveloperSettings, Engine, Foliage, Geometry*, Landscape, PhysicsCore, Projects, RenderCore, RHI; private: PCGCompute, Renderer, Voronoi, Json) | `Plugins/PCG/Source/PCG/PCG.Build.cs:13–38` |
| A grep of every PCG-family plugin (`Plugins/PCG`, `Plugins/PCGInterops`, `Plugins/Experimental/PCG*`) for `NavigationBuildLock`, `AddNavigationBuildLock`, `RemoveNavigationBuildLock`, `ENavigationBuildLock` returns **nothing** | `grep -rn` over those folders, 2026-11-21 |
| The only navigation-related lines in the PCG runtime module are three **opt-outs**: | |
| · the procedural ISM component switches navigation off in its constructor | `PCG/Private/Components/PCGProceduralISMComponent.cpp:135–136` `bNavigationRelevant = false; bCanEverAffectNavigation = false;` |
| · and overrides the query | `PCG/Private/Components/PCGProceduralISMComponent.h:117` `bool IsNavigationRelevant() const override { return false; }` |
| · the tool-volume helper's brush | `PCG/Private/Data/Tool/PCGToolVolumeData.cpp:233` `SetCanEverAffectNavigation(false)` |

**Conclusion:** PCG never locks, unlocks, rebuilds or queries navigation. Its output reaches navigation only through the engine's generic component-registration path (part B). ✔ re-checked.

## A2. Which PCG outputs can affect navigation?

### A2.1 CPU Static Mesh Spawner → ordinary (H)ISM components

| Step | Source |
|---|---|
| PCG's descriptor defaults: `ComponentClass = UInstancedStaticMeshComponent`, `Mobility = Static`, `bGenerateOverlapEvents = 0`, collision profile **`NoCollision`** | `PCG/Private/MeshSelectors/PCGISMDescriptor.cpp:11–19` |
| Its base `FSoftISMComponentDescriptor()` first calls `InitFrom(<HISM class default object>)` | `Runtime/Engine/Private/ISMPartition/ISMComponentDescriptor.cpp:54–59` |
| `InitFrom` copies `bCanEverAffectNavigation = Template->CanEverAffectNavigation()` when the cvar allows it | `ISMComponentDescriptor.cpp:150–153` |
| The cvar `ISM.ComponentDescriptor.CanEverAffectNavigation` defaults to **true** ("If true CanEverAffectNavigation property will be copied from the StaticMesh and applied to the ISM") | `ISMComponentDescriptor.cpp:18–21` |
| Mesh components can affect navigation by default: `UMeshComponent` constructor sets `bCanEverAffectNavigation = true` (the `UActorComponent` and `UPrimitiveComponent` constructors set false) | `Runtime/Engine/Private/Components/MeshComponent.cpp:32`; `ActorComponent.cpp:569`; `PrimitiveComponent.cpp:401` |
| `bCanEverAffectNavigation` is a `UPROPERTY(EditAnywhere)` on the descriptor, so a graph author can change it per mesh entry | `Runtime/Engine/Public/ISMPartition/ISMComponentDescriptor.h:236–237` |
| The descriptor applies it to the new component | `ISMComponentDescriptor.cpp:446–449` `ISMComponent->SetCanEverAffectNavigation(bCanEverAffectNavigation);` |
| PCG creates, initialises and **registers** the component before adding instances | `PCG/Private/Helpers/PCGActorHelpers.cpp:240–245` (`NewObject`, `Descriptor.InitComponent(ISMC)`, `ISMC->RegisterComponent()`) |
| The spawner then adds instances with the default `bUpdateNavigation = true` | `PCG/Private/Elements/PCGStaticMeshSpawner.cpp:887` `ISMC->AddInstances(InstanceList.Instances, false, true);` and `Runtime/Engine/Classes/Components/InstancedStaticMeshComponent.h:275` (`bool bUpdateNavigation = true`) |
| `AddInstancesInternal` computes relevance (`NumInstances > 0 && Super::IsNavigationRelevant()`) and, if relevant and registered, registers with navigation and runs a partial or full navigation update | `Runtime/Engine/Private/InstancedStaticMesh.cpp:3731–3803` |
| Relevance for a primitive: `CanEverAffectNavigation()` **and** (custom geometry, or query collision enabled **and** blocking `ECC_Pawn` or `ECC_Vehicle`) | `Runtime/Engine/Private/Components/PrimitiveComponent.cpp:3672–3687` |
| Static meshes also need `Mesh->IsNavigationRelevant()` | `Runtime/Engine/Public/StaticMeshComponentHelper.h:353–358` |

**Conclusion:**
- With PCG's **default** descriptor (`NoCollision`) the instances are **not** navigation-relevant. **[inferred]** from the two rules above.
- With a descriptor whose collision blocks Pawn (for example `BlockAll`), they **are** navigation-relevant, and every `AddInstances` call notifies the navigation system. ✔ re-checked.
- So "procedural instances never affect navigation" is **wrong as a general statement**. It is true only for the paths in A2.2.

### A2.2 GPU / procedural / componentless paths → never navigation-relevant

| Fact | Source |
|---|---|
| The GPU Static Mesh Spawner's data interface chooses a primitive factory: FastGeo (componentless) if `pcg.RuntimeGeneration.ISM.ComponentlessPrimitives` is true **and** a FastGeo factory exists, else `FPCGPrimitiveFactoryPISMC` (procedural ISM components) | `PCG/Private/Compute/DataInterfaces/Elements/PCGStaticMeshSpawnerDataInterface.cpp:53–56` (cvar, default `false`), `:1226–1240` |
| `UPCGProceduralISMComponent` hard-codes "not navigation relevant" | A1 above |
| Epic's GPU docs: GPU-spawned instances don't support "Collisions / physics" or "Navigation" ("Several features currently require CPU access to instance data and are not supported") | https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine (page labelled 5.9; opened 2026-11-21) |

### A2.3 Other outputs

- **Spawn Actor node:** spawns ordinary actors (`PCG/Private/Elements/PCGSpawnActor.cpp`); their components register with navigation like any other actor's. **[inferred]**, not traced line by line.
- **Instanced Actors interop** (`Plugins/Experimental/PCGInterops/PCGInstancedActorsInterop`, Experimental): not investigated.

## A3. The PCG frame budget (`pcg.FrameTime`) and what it covers

| Fact | Source |
|---|---|
| `pcg.FrameTime` default **5.0** ms, "Allocated time in ms per frame" | `PCG/Private/Graph/PCGGraphExecutor.cpp:81–84` |
| `pcg.EditorFrameTime` default **15.0** ms (editor, non-PIE). Epic's docs page shows 16.667 / 50 ms; the source wins for 5.8.4 | `PCGGraphExecutor.cpp:122–125`; [research/02](../Research/02_technical_challenges.md) [T10-A] |
| The budget in seconds: `CVarTimePerFrame / 1000` (game) or the editor value for editor worlds | `PCGGraphExecutor.cpp:1719–1726` (`GetTickBudgetInSeconds`) |
| The subsystem computes `EndTime = now + budget` and runs the **graph executor** with it | `PCG/Private/Subsystems/IPCGBaseSubsystem.cpp:83–104` |
| Inside `Execute`, 30% of the budget goes to processing scheduled tasks, then main-thread tasks run while `now < EndTime` (always at least one loop) | `PCGGraphExecutor.cpp:2209–2215, 2238–2252` |
| Editor-only work is excluded from the budget with `FPCGExcludeScopeFromTickBudget` | `PCGGraphExecutor.cpp:2126–2141, 2195–2225` |
| Worker-thread tasks have their own slice, `pcg.Async.OutOfTickBudgetInMilliseconds` = **5.0** | `PCG/Private/Helpers/PCGAsync.cpp:25–29` |
| The Static Mesh Spawner's `PrepareData` and `Execute` phases run **only on the main thread** (component registration reaches Chaos) | `PCG/Private/Elements/PCGStaticMeshSpawner.cpp:721–727` |

### A3.1 The order inside `UPCGSubsystem::Tick` (game thread)

`PCG/Private/Subsystems/PCGSubsystem.cpp:364–429`:

1. `RefreshAllRuntimeGenExecutionSources` if anything is dirty (`:383–391`): **before** the budget starts.
2. `Super::Tick`, `ExecuteBeginTickActions`, landscape cache tick (`:393–418`): **before** the budget starts.
3. `const double EndTime = IPCGBaseSubsystem::Tick();` (`:421`): the **executor** runs inside the budget.
4. `TrackingManager->Tick();` (`:423`).
5. `if (PCGWorldActor) RuntimeGenScheduler->Tick(PCGWorldActor, DeltaSeconds, EndTime);` (`:425–428`): the **runtime generation scheduler runs after the executor** and receives the same `EndTime`.

### A3.2 What the runtime generation scheduler does with `EndTime`

`PCG/Private/RuntimeGen/PCGRuntimeGenScheduler.cpp:332–424`:

| Step | Time-limited? | Source |
|---|---|---|
| `ShouldTick` (cvars, sleep timer, world-streaming query) | no | `:432–524` |
| `GenSourceManager->Tick()` + `GetUniqueGenSources` | no | `:356–359` |
| Virtual-texture priming, partition-actor pool | no | `:361–376` |
| 1. `TickQueueSourcesForGeneration` (grid scans, change detection) | no | `:388–400` |
| 2. `TickCleanup(..., InEndTime)` | **yes**: stops when `FPlatformTime::Seconds() >= InEndTime` (after at least one cleanup) | `:405–408`, `:1158–1166` |
| 3. `TickScheduleGeneration` | **no**. The code comment: "Only apply time budget to cleanup currently. Currently too easy to introduce latency issues so don't hold back generation new execution sources (and we already have CVarNumGeneratingComponentsAtSameTime to throttle new generations)." | `:416–418` |

**Conclusion (✔ re-checked):** `pcg.FrameTime` budgets the **graph executor**. The scheduler's own game-thread work is not counted against it and, except for cleanup, not limited by it. New generations are throttled by count, not time: `pcg.RuntimeGeneration.NumGeneratingComponents` = **16** (`:67–70`; capacity check `:1208–1215`). Navigation work caused by PCG output happens later, in the navigation system's own tick (part B), entirely outside PCG's accounting.

Epic's statement matches: Julien_LHeureux (Epic), 2026-02-20: "I don't think the budget accounts for the time spent in the runtime gen scheduler." https://forums.unrealengine.com/t/runtime-cost-in-pcg-hierarchical-generation/2708776 (opened 2026-11-21; poster on UE 5.7, PS5 test build, 500 µs PCG budget, "~1 ms" spikes).

## A4. Runtime generation cvars read in 5.8.4

| Cvar | Default | Source |
|---|---|---|
| `pcg.RuntimeGeneration.Enable` | true | `PCGRuntimeGenScheduler.cpp:57–60` |
| `pcg.RuntimeGeneration.EnableChangeDetection` | true | `:62–65` |
| `pcg.RuntimeGeneration.NumGeneratingComponents` | 16 | `:67–70` |
| `pcg.RuntimeGeneration.GlobalRadiusMultiplier` | 1.0 | `:72–75` |
| `pcg.RuntimeGeneration.EnablePooling` | true | `:82–85` |
| `pcg.RuntimeGeneration.BasePoolSize` | 100 | `:87–90` |
| `pcg.RuntimeGeneration.EnableWorldStreamingQueries` | true | `:115–118` |
| `pcg.RuntimeGeneration.FramesBeforeFirstGenerate` | 0 | `:120–123` |
| `pcg.RuntimeGeneration.TimeBetweenRuntimeGenSchedulerTicks` | 0.0 s (new in 5.8 per the release notes) | `:135–139` |
| `pcg.RuntimeGeneration.Server.Mode` | 2 ("all generation sources") | `PCG/Private/RuntimeGen/PCGGenSourceManager.cpp:36–39` |
| `pcg.Runtime.MaxExecutingThreads` | 8 | `PCGGraphExecutor.cpp:65–68` |

## A5. What runtime generation needs in the world

| Fact | Source |
|---|---|
| The scheduler ticks **only if a `APCGWorldActor` is registered** with the subsystem | `PCGSubsystem.cpp:425–428` |
| At runtime the subsystem does **not** create one: `GetPCGWorldActor()` creates it only in the editor, outside PIE | `PCGSubsystem.cpp:431–443` (`#if WITH_EDITOR … !PCGHelpers::IsRuntimeOrPIE()`) |
| `APCGWorldActor` is `UCLASS(MinimalAPI, NotBlueprintable, NotPlaceable)` and registers itself in `BeginPlay` | `PCG/Public/PCGWorldActor.h:16–17`; `PCG/Private/PCGWorldActor.cpp:187–191` |
| Player controllers are generation sources by default (`bTreatPlayerControllersAsGenerationSources = true`) | `PCGWorldActor.h:80–82`; `PCGGenSourceManager.cpp:203–233` |
| `PartitionGridSize` default 25,600 cm (256 m) | `PCGWorldActor.h:59–63` |

**Consequence [inferred, to verify in S1]:** a map without a saved `APCGWorldActor` needs one spawned at runtime before runtime generation can work. Spawning it is plausible (it registers in `BeginPlay`), but nothing in the source says it is supported. Step S1 tests it in a packaged build.

## A6. Component API used by the design

| Fact | Source |
|---|---|
| `EPCGComponentGenerationTrigger`: `GenerateOnLoad`, `GenerateOnDemand`, `GenerateAtRuntime` ("Generates only when scheduled by the Runtime Generation Scheduler") | `PCG/Public/PCGComponent.h:77–82` |
| Default trigger `GenerateOnLoad`; default `Seed = 42`; `bIsComponentPartitioned = false` | `PCGComponent.h:314, 324, 327` |
| Native multicast delegates `OnPCGGraphStartGeneratingDelegate`, `OnPCGGraphCancelledDelegate`, `OnPCGGraphGeneratedDelegate`, `OnPCGGraphCleanedDelegate`, each with `UPCGComponent*` | `PCGComponent.h:55–58, 380–383` |
| The subsystem-level "generation done" delegates exist **only in editor builds** (`#if WITH_EDITOR`) | `PCG/Public/Subsystems/PCGSubsystem.h:52–71` |
| `SetGraph`, `GenerateLocal`, `SetSchedulingPolicyClass`, `GetGenerationRadii` | `PCGComponent.h:191–296` |
| `bOverrideGenerationRadii` + `GenerationRadii` (only for `GenerateAtRuntime`) | `PCGComponent.h:334–338` |
| `[EXPERIMENTAL]` actor/componentless runtime generation ("Avoids creating partition actors and local PCG components") | `PCGComponent.h:849–850`; `PCG/Public/PCGGraph.h:805` |
| Graphs can be built in code at runtime: `UPCGGraph::AddNodeOfType`, `AddEdge`, `GetInputNode`, `GetOutputNode` are outside `WITH_EDITOR` | `PCG/Public/PCGGraph.h:454, 477, 485, 489` |
| Settings used by the planned graph: `UPCGCreatePointsGridSettings` (`GridExtents`, `CellSize`, `CoordinateSpace`, `PointPosition`) | `PCG/Public/Elements/PCGCreatePointsGrid.h:28–89` |
| `UPCGStaticMeshSpawnerSettings::SetMeshSelectorType`; `UPCGMeshSelectorWeighted::MeshEntries` of `FPCGMeshSelectorWeightedEntry { Descriptor, Weight }` | `PCG/Public/Elements/PCGStaticMeshSpawner.h:66–77`; `PCG/Public/MeshSelectors/PCGMeshSelectorWeighted.h:24–39, 100` |
| Generated components get the tag `"PCG Generated Component"` | `PCG/Public/Helpers/PCGHelpers.h:35`; `PCGActorHelpers.cpp:117` |

## A7. Determinism inputs

| Fact | Source |
|---|---|
| Node seeds combine the node's seed with the execution source's seed | `PCG/Private/PCGSettings.cpp:723–726` |
| Position-based seeds truncate the position to `int` per axis | `PCG/Private/Helpers/PCGHelpers.cpp:50–53` (`ComputeSeedFromPosition`) |
| PCG has its own determinism test suite (native and Blueprint-based) | `PCG/Private/Tests/Determinism/` (7 files) |
| Epic forum answer (2026, snippet only): "minor differences based on hardware/platform" | [research/03](../Research/03_existing_solutions_gap_analysis.md) §11.1 |

**[inferred]** Same binary + same machine + same seeds should give identical content. Two risks to measure: multithreaded execution changing the **order** of instances inside an ISM, and float differences between the editor and packaged builds.

## A8. Maturity in 5.8.4 (`.uplugin` flags read today, plus labels in code and docs)

| Feature | Label | Evidence |
|---|---|---|
| PCG framework (plugin `PCG`) | no Beta/Experimental flag; **EnabledByDefault** | `Plugins/PCG/PCG.uplugin:13–15` |
| "Production-Ready" | since 5.7 | Epic, "Unreal Engine 5.7 is here!": https://www.unrealengine.com/news/unreal-engine-5-7-is-now-available |
| GPU processing | **Beta** ("Learn to use this Beta feature, but use caution when shipping with it") | GPU docs page above |
| GPU Static Mesh Spawner | **Experimental** ("The GPU implementation is Experimental and not all Static Mesh Spawner features are supported") | GPU docs page above |
| Actor/componentless runtime generation | **Experimental** | `PCGComponent.h:849` |
| Copy Points GPU-only options, World Raycast UV/normal outputs | **Experimental** | `PCG/Public/Elements/PCGCopyPoints.h:105, 112`; `PCGWorldRaycast.h:111–119` |
| PCGFastGeoInterop, PCGInstancedActorsInterop, PCGNaniteAssembliesInterop, PCGNiagaraInterop, PCGWaterInterop | **Experimental** plugins | each `.uplugin:16` `"IsExperimentalVersion": true` |
| PCGBiomeCore, PCGBiomeSample, PCGMeshPartitionInterop, PCGPrimitives, PCGToolset | **Experimental** plugins | their `.uplugin` files |
| PCGGeometryScriptInterop, PCGExternalDataInterop, PCGPythonInterop | **Beta** plugins | each `.uplugin:15` `"IsBetaVersion": true` |

The 5.8 release notes (https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes, search snippet only) list scheduler optimisations ("up to 30%"), the new `TimeBetweenRuntimeGenSchedulerTicks` cvar and runtime generation without actors or components.
