> **Raw Phase 2 research notes (C_existing_work), 2026-11-21.** Kept as an evidence trail. Web sources have URLs and the date they were opened; "snippet only" means only a search-result summary was read. **The findings file [../01b_Research_Findings.md](../01b_Research_Findings.md) is authoritative.**

# P08 NavReadyPCG: Phase 2 research, part C: existing solutions and what they leave unsolved

## C1. Inside the engine (UE 5.8.4)

| Existing piece | What it does | What it does not do | Evidence |
|---|---|---|---|
| PCG runtime generation scheduler | Generates runtime components around generation sources, in priority order, at most `pcg.RuntimeGeneration.NumGeneratingComponents` (16) at a time; pools partition actors; cleans up out of range | Has no notion of navigation. Its own tick is outside `pcg.FrameTime` (only cleanup checks the deadline) | notes A3–A5 |
| `pcg.FrameTime` (5 ms) | Time-slices the graph executor on the game thread | Doesn't cover the scheduler, nor navigation work caused by the output | notes A3 |
| GPU processing (Beta) and the GPU Static Mesh Spawner (Experimental) | Fast scatter on the GPU | Instances have no collision and no navigation (Epic docs) | notes A2.2, A8 |
| FastGeo interop (Experimental) | Componentless primitives | Not navigation-relevant (same procedural path) **[inferred]** | notes A2.2 |
| Dynamic navmesh (`RuntimeGeneration=Dynamic`) | Rebuilds dirty tiles at runtime on worker threads | Rebuilds whenever content changes, with no idea whether the content is "finished"; readiness signals are global | notes B5–B6 |
| Dynamic Modifiers Only + navigation modifiers | Cheap: only area/obstacle changes on top of a **pre-built** base navmesh | Can't create new navmesh for generated geometry; needs a base navmesh built in advance | notes B5 (forum 2693069) |
| `AddNavigationBuildLock` / `RemoveNavigationBuildLock` | Pause dirty-area processing and tile generation globally | Default release calls `RebuildAll()`, which discards and rebuilds the whole navmesh; with `NoRebuild` the lock is still global, so one generating cell blocks every other cell's rebuild | notes B1–B2 |
| `bInitialBuildingLocked` + `ReleaseInitialBuildingLock()` (Epic's documented pattern) | Same lock, using the `InitialLock` flag | `ReleaseInitialBuildingLock` calls `RemoveNavigationBuildLock(InitialLock)` with the default `Rebuild` (`NavigationSystem.cpp:4094–4097`), so a full rebuild in 5.8.4. The docs' tip "call `DefaultDirtyAreasController.Reset()`" needs a subclass: the member is protected (`NavigationSystem.h:1237, 1262`) | Epic docs, "Optimizing Navigation Mesh Generation Speed" (labelled 5.9; opened 2026-11-21): https://dev.epicgames.com/documentation/en-us/unreal-engine/optimizing-navigation-mesh-generation-speed-in-unreal-engine. Quotes: "set your navigation system's bInitialBuildingLocked to True", "To unlock the generation, call the function ReleaseInitialBuildingLock()", "Once unlocked, the Navigation Mesh will rebuild all the tiles marked as dirty by the loaded assets." |
| `ANavigationData::SetRebuildingSuspended` | Per navigation data: caches dirty areas while suspended | Still global for that navigation data; doesn't stop tiles already in progress; past 32,768 cached areas it rebuilds everything | notes B7 |
| `IsNavigationBuildInProgress()`, `OnNavigationGenerationFinishedDelegate` | "Is navigation busy?" / "did it finish?" | **Global**: under continuous generation, "finished" may never be reported while some other area is still building | notes B6 |
| `FRecastNavMeshGenerator::HasDirtyTiles(FBox)` | Per-area "are tiles still pending or running here?" | Only a query: nothing in the engine uses it to gate AI | notes B6 |
| Path invalidation on tile rebuild | `OnNavMeshTilesUpdated` invalidates paths crossing rebuilt tiles, so path followers can re-path | Doesn't stop new queries against stale tiles before the rebuild | notes B6 |

## C2. Epic forum threads (the "workaround")

**1. "DynamicModifierOnly and Runtime navmesh building (PCG)"**, Dec 2025. https://forums.unrealengine.com/t/dynamicmodifieronly-and-runtime-navmesh-building-pcg/2693069 (opened 2026-11-21)
- Problem: PCG builds the level; with Dynamic Modifiers Only no navmesh appeared after PCG finished.
- Reply by James Keeling (the page does not show whether he is Epic staff, so this is **[unverified]**): "Dynamic navmesh generation is the only mode that will fully create navmesh at runtime." His suggestion, paraphrased by the fetch: lock the navmesh while PCG runs, then release the lock and rebuild the navmesh in the dirty area afterward; adjust `FNavRegenTimeSliceManager` values; or use Dynamic Modifiers Only on top of a loaded base navmesh, with modifiers on PCG objects.
- **No code and no arguments were given.** So "the forum nav-lock workaround" is a one-line suggestion, not a published, measured implementation.

**2. "Navmesh runtime regeneration [AddNavigationBuildLock]"**, 2026-03-08. https://forums.unrealengine.com/t/navmesh-runtime-regeneration-addnavigationbuildlock/2704680 (opened 2026-11-21)
- **Not about PCG.** The poster moves objects once in `BeginPlay`, set the navmesh to Dynamic, and locked navigation "after the movement thing". Flags and the release action are not stated. "i got 20 frames back doing it this way" has no unit or baseline. No replies.
- **Correction to the portfolio research:** [research/03](../Research/03_existing_solutions_gap_analysis.md) §11.3 used to say "A user who did this reported '20 frames back'", which read as if the user applied the lock workaround to PCG. They did not: the quote is from this unrelated thread. Reported from this Phase 2 and corrected in the portfolio research file on 2026-11-21; the copy in `Research\` is the corrected version.

**3. "Runtime Cost in PCG Hierarchical Generation"**, Dec 2025 – Feb 2026. https://forums.unrealengine.com/t/runtime-cost-in-pcg-hierarchical-generation/2708776 (opened 2026-11-21)
- Poster: UE 5.7, PS5 test build, "we are budgeting 500us of our CPU time for PCG", still "spikes of ~1 ms".
- Julien_LHeureux (Epic): "I don't think the budget accounts for the time spent in the runtime gen scheduler"; "there are some levers in the runtime scheduler so that we don't try to generate more than a set number of components", which "could help removing spiking too"; "We're doing significant improvements on the runtime gen scheduler as part of 5.8."

## C3. Official documentation and release notes

| Source | What it says | Opened |
|---|---|---|
| "Unreal Engine 5.7 is here!": https://www.unrealengine.com/news/unreal-engine-5-7-is-now-available | PCG framework is Production-Ready | 2026-11-21 (search summary) |
| UE 5.8 release notes: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes | Scheduler up to 30% less game-thread time; `pcg.RuntimeGeneration.TimeBetweenRuntimeGenSchedulerTicks`; runtime generation without actors/components; PCG enabled by default | 2026-11-21 (search summary) |
| "Using PCG with GPU Processing": https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine | GPU processing is Beta; GPU Static Mesh Spawner is Experimental; GPU instances support no collision/physics and no navigation | 2026-11-21 (page labelled 5.9) |
| "Optimizing Navigation Mesh Generation Speed": URL above | Lock/release pattern; tile size advice (32–128 cells per side); `MaxSimultaneousTileGenerationJobsCount` limited by worker threads | 2026-11-21 (page labelled 5.9) |

**Version mismatch:** both doc pages are served as 5.9. Where they disagree with the 5.8.4 source (for example the release-lock behaviour), the source wins.

## C4. Community

| Item | What it does | Status of our evidence |
|---|---|---|
| "Runtime Navigation Area Modifiers" (Gumroad, Wouter Weynants): https://wouterweynants.gumroad.com/l/navigation | PCG-integrated navigation **area** modifiers, not rebuild orchestration | snippet only ([research/03](../Research/03_existing_solutions_gap_analysis.md) §11.4) |
| GPU-PCG (KisaragiTomi): https://github.com/KisaragiTomi/GPU-PCG | GPU PCG nodes, UE 5.7 source build | snippet only |
| General "navmesh not updating for runtime geometry" articles and old forum threads, e.g. https://forums.unrealengine.com/t/add-rebuild-dirty-areas-only-for-navmesh-in-c/133870 | Ask for a "rebuild dirty areas only" API; suggest `RebuildNavigation` console command or `AddDirtyArea` | search snippets only, 2026-11-21 |

A web search on 2026-11-21 found **no** plugin or published system that tracks per-region navmesh readiness for PCG output and gates AI on it. A search can't prove absence, so this is a statement about what was found, never "nobody has done this".

## C5. The gap, restated after Phase 2

What exists: generation (PCG), rebuilding (Dynamic navmesh), a global pause (locks, suspend), global "done" signals, and a per-area dirty-tile query.

What doesn't exist in the engine or in the public sources found:
1. a per-cell handshake "content finished → handed to navigation → tiles rebuilt → AI may use it";
2. a way to hand content to navigation per cell, at a chosen moment, without a global lock;
3. a measurement of what the uncoordinated default costs (path failures near fresh content, time-to-navigable, worst frame);
4. a determinism check on the generated output.

**Gap strength: Medium.** Lower than the roadmap's "Medium–Strong": the building blocks are all public engine APIs, and an experienced engineer could assemble the handshake. The value is in assembling it correctly (the default release rebuilds everything; the global signals mislead under load) and in measuring it honestly.
