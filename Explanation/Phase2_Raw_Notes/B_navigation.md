> **Raw Phase 2 research notes (B_navigation), UE 5.8.4, 2026-11-21.** Kept as an evidence trail. Every claim cites a `File:line`. **The claims the design depends on were re-checked by hand and are restated in [../01b_Research_Findings.md](../01b_Research_Findings.md). Treat anything here that is not restated there as unverified.**

# P08 NavReadyPCG: Phase 2 research, part B: navigation build locks, dirty areas and tile rebuilds (UE 5.8.4)

- Paths are relative to `Engine/`. `Nav/` means `Engine/Source/Runtime/NavigationSystem/`.
- **[inferred]** marks conclusions drawn from following the code flow.

---

## B1. Build locks

| Fact | Source |
|---|---|
| Lock flags: `NoUpdateInEditor = 1<<1`, `NoUpdateInPIE = 1<<2`, `InitialLock = 1<<3` ("release manually after levels are ready for rebuild (e.g. streaming)"), **`Custom = 1<<4`**, `AsyncLoadLock = 1<<5` | `Nav/Public/NavigationSystem.h:102–112` |
| `IsNavigationBuildingLocked(uint8 Flags = ~0)` tests the combined flags | `NavigationSystem.h:1083` |
| `enum class ELockRemovalRebuildAction { Rebuild, RebuildIfNotInEditor, NoRebuild }`; `AddNavigationBuildLock(uint8 Flags)`; `RemoveNavigationBuildLock(uint8 Flags, ELockRemovalRebuildAction RebuildAction = Rebuild)` (both `NAVIGATIONSYSTEM_API`, public) | `NavigationSystem.h:1146–1153` |
| `AddNavigationBuildLock` ORs the flags in; on the unlocked→locked edge it only tells the dirty-area controller (which uses it to silence the oversized-area warning) | `Nav/Private/NavigationSystem.cpp:4798–4812`; `Nav/Private/NavigationDirtyAreasController.cpp:237–249, 270–274` |
| `RemoveNavigationBuildLock` clears the flags; on the locked→unlocked edge, with `Rebuild` (**the default**) or `RebuildIfNotInEditor` in a game, it calls **`RebuildAll()`** | `NavigationSystem.cpp:4814–4836` (`RebuildAll();` at `:4833`) |
| `RebuildAll` gathers bounds and calls `NavData->RebuildAll()` on every navigation data object that supports runtime generation | `NavigationSystem.cpp:4846–4879` |
| The engine itself removes its own locks with `NoRebuild` in several places (initial lock after setup, PIE lock, async-load lock) | `NavigationSystem.cpp:1391, 1453, 4794, 5997` |
| The initial lock is added at setup when `bInitialBuildingLocked` is set | `NavigationSystem.cpp:1171–1178` |

**Conclusion (✔ re-checked):** releasing a lock with the default argument **rebuilds the whole navmesh**, not only the dirty areas. To rebuild only what changed, pass `ELockRemovalRebuildAction::NoRebuild`; the dirty areas that accumulated during the lock are then processed by the next navigation tick (B2).

## B2. What a lock actually stops: `UNavigationSystemV1::Tick`

`Nav/Private/NavigationSystem.cpp:1633–1811`, in order:

1. Pending bounds updates and nav-data registration (`:1649–1659`).
2. **Pending octree updates are processed even while locked** (`:1662–1673`, `NavHandler.ProcessPendingOctreeUpdates()` at `:1671`). This is where newly registered components become octree elements and produce dirty areas.
3. `if (IsNavigationBuildingLocked() == false)` (`:1676`):
   - `RebuildDirtyAreas(DeltaSeconds)` (`:1684–1687`) → `FNavigationDirtyAreasController::Tick` (`NavigationSystem.cpp:4881–4887`);
   - `TickAsyncBuild` of every navigation data (`:1690–1794`).
4. Per-frame CSV stats `NavTasks/NumRemainingTasks_<NavData>` (category `NavTasks`, on by default) and `NavigationSystem/NumRunningTasks` (`:1796–1811`; categories defined at `:175–178`).

So a lock lets dirty areas **accumulate** and pauses tile generation (including collecting finished tiles). ✔ re-checked.

**Where the navigation tick sits in the frame:** `UWorld::Tick` calls `NavigationSystem->Tick(DeltaSeconds)` inside `CSV_SCOPED_TIMING_STAT_EXCLUSIVE(NavigationBuild)`, **before** the actor tick groups (`Runtime/Engine/Private/LevelTick.cpp:1641–1647`; `TG_PrePhysics` at `:1750`). Tickable objects, which include the PCG subsystem, run after `TG_PostPhysics` (`:1778`, `:1821`). **[inferred]** So PCG output produced in frame N is first seen by navigation at the start of frame N+1.

## B3. Dirty areas

| Fact | Source |
|---|---|
| `DirtyAreasUpdateFreq = 60.f` (Hz): dirty areas are flushed to the generators at most 60 times per second | `Nav/Public/NavigationDirtyAreasController.h:25`; `NavigationDirtyAreasController.cpp:53–56` |
| On flush, every navigation data gets `RebuildDirtyAreas(DirtyAreas)`, then the list is reset | `NavigationDirtyAreasController.cpp:101–110` |
| Areas are stored only if their flags are non-empty and accumulation is allowed; empty or invalid bounds are skipped with a warning | `NavigationDirtyAreasController.cpp:161–233` |
| Public queries: `HasDirtyAreasQueued()`, `GetNumDirtyAreas()`, `AddDirtyArea(...)` | `NavigationSystem.h:973–977`; `NavigationSystem.cpp` (`HasDirtyAreasQueued` returns `DefaultDirtyAreasController.IsDirty()`) |
| The controllers themselves are **protected** members (`DefaultOctreeController`, `DefaultDirtyAreasController`) | `NavigationSystem.h:1237, 1258, 1262` |

## B4. Component → octree → dirty area

| Fact | Source |
|---|---|
| `FNavigationSystem::OnComponentRegistered / OnComponentUnregistered / RegisterComponent / UpdateComponentData` are the engine-side entry points | `Runtime/Engine/Classes/AI/NavigationSystemBase.h:136–153` |
| Registration is **deferred**: `RegisterElementWithNavOctree` adds a `FNavigationDirtyElement` to `PendingUpdates` (or ignores it if the octree is locked) | `Nav/Private/NavigationDataHandler.cpp:68–118` |
| `ProcessPendingOctreeUpdates` drains **all** pending updates in one call (no time budget) | `NavigationDataHandler.cpp:581–598` |
| There is no public accessor for the number of pending octree updates | `Nav/Public/NavigationOctreeController.h:37` (`PendingUpdates` is a member of the protected controller) |
| Default geometry gathering mode: `ENavDataGatheringModeConfig::Instant` (geometry exported when the element enters the octree) | `NavigationSystem.cpp:937`; enum at `Runtime/Engine/Classes/AI/Navigation/NavigationTypes.h:72–77` |
| `UActorComponent::SetCanEverAffectNavigation(bool)` on a **registered** component calls `FNavigationSystem::OnComponentRegistered` (true) or `OnComponentUnregistered` (false) | `Runtime/Engine/Private/Components/ActorComponent.cpp:3266–3296` |
| For an ISM, the first instance added to a relevant, registered component triggers `FNavigationSystem::RegisterComponent`, then a partial or full navigation update | `Runtime/Engine/Private/InstancedStaticMesh.cpp:3659–3680, 3731–3803` |

**Consequence [inferred]:** a PCG ISM spawned with `bCanEverAffectNavigation = false` is invisible to navigation; calling `SetCanEverAffectNavigation(true)` later puts it into the octree at the next navigation tick, which marks its bounds dirty. That is a **per-component** way to choose *when* navigation sees new content, without a global lock. To be verified by test in step S5.

## B5. Runtime generation modes and defaults

| Fact | Source |
|---|---|
| `ERuntimeGenerationType { Static, DynamicModifiersOnly, Dynamic, LegacyGeneration }`; "Dynamic: Fully dynamic, supports geometry changes along with navigation modifiers" | `Nav/Public/NavigationData.h:523–533` |
| Engine default: `[/Script/NavigationSystem.NavigationData] RuntimeGeneration=Static` | `Engine/Config/BaseEngine.ini:3042–3043` |
| `ARecastNavMesh::SupportsRuntimeGeneration()` = not Static (or a one-time initial rebuild) | `Nav/Private/NavMesh/RecastNavMesh.cpp:4379–4383` |
| Recast defaults: `TileSizeUU=1000.f`, `CellSize=19.f`, `AgentRadius=34.f`, `AgentHeight=144.f` | `BaseEngine.ini:3045–3066` |
| `MaxSimultaneousTileGenerationJobsCount = 1024`, `bDoFullyAsyncNavDataGathering = false` (geometry gathered on the game thread) | `RecastNavMesh.cpp:549–550` |
| Epic's forum answer (James Keeling, role not confirmed on the page): "Dynamic navmesh generation is the only mode that will fully create navmesh at runtime"; Dynamic Modifiers Only works with PCG only on top of a loaded base navmesh | https://forums.unrealengine.com/t/dynamicmodifieronly-and-runtime-navmesh-building-pcg/2693069 |

## B6. Tile generation (Recast)

| Fact | Source |
|---|---|
| `RECAST_ASYNC_REBUILDING` defaults to 1 and `ALLOW_TIME_SLICE_NAV_REGEN` to 0, so `TIME_SLICE_NAV_REGEN` is 0: tiles are built by **async worker tasks**, and the time-sliced regeneration manager is inactive | `Nav/Public/NavMesh/RecastNavMesh.h:28–40`; `NavigationSystem.cpp:595–607` |
| Worker cap: `MaxTileGeneratorTasks = min(max(2 × worker threads, 1), MaxSimultaneousTileGenerationJobsCount)` | `Nav/Private/NavMesh/RecastNavMeshGenerator.cpp:5465` |
| With game-thread gathering (the default) the generator submits up to `MaxTileGeneratorTasks − running` tasks per tick; with async gathering, only 1 | `RecastNavMeshGenerator.cpp:5891–5897` |
| Finished tiles are applied in `TickAsyncBuild` on the game thread, then `OnNavMeshTilesUpdated(UpdatedTileRefs)` | `RecastNavMeshGenerator.cpp:5865–5905` |
| `ARecastNavMesh::OnNavMeshTilesUpdated` **invalidates active paths that cross the rebuilt tiles** | `RecastNavMesh.cpp:2986–2989` |
| When the last pending tile finishes: `DestNavMesh->OnNavMeshGenerationFinished()` → `UNavigationSystemV1::OnNavigationGenerationFinished` → `OnNavigationGenerationFinishedDelegate` (BlueprintAssignable) | `RecastNavMeshGenerator.cpp:7631`; `RecastNavMesh.cpp:3169–3242`; `NavigationSystem.h:443–444` |
| **Per-area query:** `FRecastNavMeshGenerator::HasDirtyTiles(const FBox& AreaBounds)` checks pending **and** running dirty tiles inside the box; `GetDirtyTilesCount(const FBox&)` counts them (both public, `NAVIGATIONSYSTEM_API`) | `Nav/Public/NavMesh/RecastNavMeshGenerator.h:829–831`; `RecastNavMeshGenerator.cpp:6644–6669` |
| Global queries: `IsNavigationBuildInProgress()` (any generator has pending or running tiles), `GetNumRemainingBuildTasks()` | `NavigationSystem.h:1101, 1106`; `NavigationSystem.cpp:4889–4911` |
| Access: `GetDefaultNavDataInstance(FNavigationSystem::DontCreate)` → `ANavigationData::GetGenerator()` | `NavigationSystem.h:743`; `Nav/Public/NavigationData.h:711–712` |

**Consequence [inferred]:** "is this cell navigable yet?" can be answered per cell, with no engine change, as: at least one navigation tick has run since the cell's content was handed to navigation **and** no dirty areas are queued **and** building isn't locked **and** `HasDirtyTiles(CellBounds)` is false. The engine's own signals (`IsNavigationBuildInProgress`, `OnNavigationGenerationFinished`) are **global**.

## B7. Suspending one navigation data (an alternative to locks)

| Fact | Source |
|---|---|
| `ANavigationData::SetRebuildingSuspended(bool)` caches every `RebuildDirtyAreas` call while suspended and pushes them when resumed. "Note that calling SetRebuildingSuspended(true) won't suspend the nav generation already in progress." "Leaving generation suspended for extended periods of time can trigger a RebuildAll()" | `Nav/Public/NavigationData.h:717–729` |
| The cap is `ai.navigation.MaxDirtyAreasCountWhileSuspended` = **32768**; past it, the whole navigation data is rebuilt on resume | `Nav/Private/NavigationData.cpp:38–40, 681–700` |

## B8. Path queries used by the agents and probes

| Fact | Source |
|---|---|
| `FPathFindingResult FindPathSync(FPathFindingQuery Query, EPathFindingMode::Type Mode = Regular)` (and an overload with agent properties) | `NavigationSystem.h:650, 658` |
| `FindPathAsync(...)` returns a query id | `NavigationSystem.h:668` |
| `FPathFindingResult { FNavPathSharedPtr Path; ENavigationQueryResult::Type Result; IsSuccessful(); IsPartial(); }` | `Nav/Public/NavigationData.h:57–70` |
| `ENavigationQueryResult { Invalid, Error, Fail, Success }` | `Runtime/Engine/Classes/AI/Navigation/NavigationTypes.h:638–647` |
| Sync path finding is timed in CSV as `NavigationSystem/PathfindingSync` (category off by default) | `NavigationSystem.cpp:175, 1890` |

## B9. Navigation CSV stats (for measurement)

| Stat | Category / default | Source |
|---|---|---|
| `Exclusive/GameThread/NavigationBuild` (the whole navigation tick) | `Exclusive`, on | `LevelTick.cpp:1645`; `Core/Private/ProfilingDebugging/CsvProfiler.cpp:73` |
| `Navigation_ProcessPendingOctreeUpdates`, `Navigation_RebuildDirtyAreas`, `Navigation_TickAsyncBuild`, … | `NavigationBuildDetailed`, **on** | `NavigationSystem.cpp:176, 1664, 1685, 1692` |
| `NumRunningTasks` | `NavigationSystem`, off | `NavigationSystem.cpp:175, 1810` |
