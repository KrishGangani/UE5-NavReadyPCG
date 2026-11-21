// NavReadyPCGCore - UNavReadySubsystem.

#include "NavReadySubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Helpers/PCGHelpers.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavMesh/RecastNavMeshGenerator.h"
#include "NavReadyCellComponent.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/Stats.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavReadySubsystem)

// The probe reads Recast's dirty tiles; every desktop target compiles Recast (Target.bCompileRecast, NavigationSystem.Build.cs:40-43).
static_assert(WITH_RECAST, "NavReadyPCGCore needs Recast navigation.");
// HasDirtyTiles(FBox) sees pending and running tiles; a tile in the middle of a time-sliced rebuild is in neither
// (RecastNavMeshGenerator.cpp:6588-6593 vs 6644-6670). Time slicing is only used without async rebuilding, the default
// (RecastNavMesh.h:28-32), so the probe requires it.
static_assert(RECAST_ASYNC_REBUILDING, "NavReadyPCGCore's readiness probe needs asynchronous Recast tile rebuilding.");

CSV_DEFINE_CATEGORY(NavReady, true);

// ECVF_Default: cheat cvars can't be set from config or -dpcvars.
static TAutoConsoleVariable<int32> CVarCommitsPerFrame(TEXT("nr.CommitsPerFrame"), 1,
	TEXT("Most cells NavReady hands to navigation per frame (at least 1)."), ECVF_Default);

namespace NavReadyPrivate
{
	/** The instanced static mesh components PCG generated on the actor (PCG tags them, PCGHelpers::DefaultPCGTag). */
	static void GetGeneratedComponents(const AActor& Owner, TArray<UInstancedStaticMeshComponent*>& Out)
	{
		Owner.GetComponents(Out);
		Out.RemoveAll([](const UInstancedStaticMeshComponent* Component) { return !Component->ComponentHasTag(PCGHelpers::DefaultPCGTag); });
	}
}

bool UNavReadySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

int32 UNavReadySubsystem::IndexOf(const UNavReadyCellComponent& Component) const
{
	return Components.IndexOfByKey(&Component);
}

const FNavReadyCell* UNavReadySubsystem::FindCell(const UNavReadyCellComponent& Component) const
{
	const int32 Index = IndexOf(Component);
	return Index == INDEX_NONE ? nullptr : &Cells[Index];
}

void UNavReadySubsystem::RegisterCell(UNavReadyCellComponent& Component)
{
	if (IndexOf(Component) == INDEX_NONE)
	{
		Cells.AddDefaulted_GetRef().Id = NextCellId++;
		Components.Add(&Component);
	}
}

void UNavReadySubsystem::UnregisterCell(UNavReadyCellComponent& Component)
{
	const int32 Index = IndexOf(Component);
	if (Index == INDEX_NONE)
	{
		return;
	}
	if (bBroadcasting)
	{
		Components[Index] = nullptr; // removed by Compact() once the broadcasts are done
		Cells[Index] = FNavReadyCell();
	}
	else
	{
		Cells.RemoveAtSwap(Index);
		Components.RemoveAtSwap(Index);
	}
}

void UNavReadySubsystem::NotifyGenerationStarted(UNavReadyCellComponent& Component)
{
	const int32 Index = IndexOf(Component);
	if (Index != INDEX_NONE)
	{
		Cells[Index].OnGenerationStarted(FPlatformTime::Seconds());
		QueueBroadcast(Index);
	}
}

void UNavReadySubsystem::NotifyGenerated(UNavReadyCellComponent& Component)
{
	const int32 Index = IndexOf(Component);
	if (Index != INDEX_NONE)
	{
		if (Cells[Index].OnGenerated(FPlatformTime::Seconds()))
		{
			QueueBroadcast(Index);
		}
	}
}

void UNavReadySubsystem::NotifyReset(UNavReadyCellComponent& Component)
{
	const int32 Index = IndexOf(Component);
	if (Index != INDEX_NONE)
	{
		Cells[Index].OnReset();
		QueueBroadcast(Index);
	}
}

void UNavReadySubsystem::QueueBroadcast(int32 Index)
{
	PendingBroadcasts.Emplace(Components[Index].Get(), Cells[Index].State);
}

void UNavReadySubsystem::FlushBroadcasts()
{
	bBroadcasting = true;
	// By index, and by copy: a listener can queue more changes (a cleanup it triggers) and so grow the array.
	for (int32 Index = 0; Index < PendingBroadcasts.Num(); ++Index)
	{
		const TPair<TWeakObjectPtr<UNavReadyCellComponent>, ENavReadyCellState> Change = PendingBroadcasts[Index];
		if (UNavReadyCellComponent* Component = Change.Key.Get())
		{
			Component->OnCellStateChanged.Broadcast(Component, Change.Value);
			OnAnyCellStateChanged.Broadcast(Component, Change.Value);
		}
	}
	PendingBroadcasts.Reset();
	bBroadcasting = false;
	Compact();
}

void UNavReadySubsystem::Compact()
{
	for (int32 Index = Components.Num() - 1; Index >= 0; --Index)
	{
		if (!Components[Index])
		{
			Cells.RemoveAtSwap(Index);
			Components.RemoveAtSwap(Index);
		}
	}
}

void UNavReadySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(NavReady);

	const double Now = FPlatformTime::Seconds();
	const uint64 Frame = GFrameCounter;

	// The probe's global part, once per frame. The generator is fetched every frame, never cached: a full rebuild can
	// recreate it (NavigationData.cpp:626). It is a Recast generator because the navigation data is a Recast navmesh.
	FNavReadyProbe Probe;
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ARecastNavMesh* NavMesh = NavSys ? Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)) : nullptr;
	const FRecastNavMeshGenerator* Generator = NavMesh ? static_cast<const FRecastNavMeshGenerator*>(NavMesh->GetGenerator()) : nullptr;
	Probe.bNavigationAvailable = Generator != nullptr;
	if (Generator)
	{
		Probe.bBuildLocked = NavSys->IsNavigationBuildingLocked();
		Probe.bDirtyAreasQueued = NavSys->HasDirtyAreasQueued();
	}

	int32 CountByState[static_cast<int32>(ENavReadyCellState::AIReady) + 1] = {};
	int32 CommitsLeft = FMath::Max(1, CVarCommitsPerFrame.GetValueOnGameThread());
	int32 CommitsThisFrame = 0;
	int32 DirtyWhileReady = 0;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		FNavReadyCell& Cell = Cells[Index];
		const UNavReadyCellComponent* Component = Components[Index];
		if (!IsValid(Component))
		{
			continue;
		}

		if (Cell.State == ENavReadyCellState::Generated && Generator && CommitsLeft > 0)
		{
			// S3 observes only: in the baselines the props reached navigation when PCG spawned them, so the "commit" is the
			// moment NavReady sees the cell generated. The readiness bounds are the cell's content grown by the navmesh's
			// dirty-area border: every tile a change in the cell can mark dirty, including its neighbours' edge tiles.
			// (Only once a generator exists: bounds taken without the border would stay too small for good.) The engine
			// grows dirty areas by (High, Low) and GrowBoundingBox by (Low, High) (RecastNavMeshGenerator.cpp:6596-6608), so
			// the larger of the two growths is used on every side.
			const FBox ContentBounds = Component->GetOwner()->GetComponentsBoundingBox(/*bNonColliding=*/true);
			const FBox Grown = Generator->GrowBoundingBox(ContentBounds, /*bIncludeAgentHeight=*/false);
			const FVector Growth = FVector::Max(ContentBounds.Min - Grown.Min, Grown.Max - ContentBounds.Max);
			Cell.ReadinessBounds = FBox(ContentBounds.Min - Growth, ContentBounds.Max + Growth);
			// The commit: the cell's generated components become navigation-relevant, all at once, so navigation rebuilds
			// the cell's tiles once. In the baselines they already are (spawned with the flag on): nothing to switch.
			TArray<UInstancedStaticMeshComponent*> Generated;
			NavReadyPrivate::GetGeneratedComponents(*Component->GetOwner(), Generated);
			for (UInstancedStaticMeshComponent* Instances : Generated)
			{
				if (!Instances->CanEverAffectNavigation())
				{
					Instances->SetCanEverAffectNavigation(true);
					// Registering an ISM that already has its instances does not dirty its area: ISMs opt out of the
					// dirty area on add, expecting per-instance updates (InstancedStaticMesh.cpp:5513-5519,
					// NavigationDataHandler.cpp:50). Found in the final batch: NR's props never blocked. So the commit
					// dirties the component's bounds itself. Verified: job 20261121_112721_9a0628, 16/16 cells block.
					NavSys->AddDirtyArea(Instances->Bounds.GetBox(), ENavigationDirtyFlag::All, TEXT("NavReadyCommit"));
				}
			}
			--CommitsLeft;
			++CommitsThisFrame;
			if (Cell.OnCommitted(Now, Frame))
			{
				QueueBroadcast(Index);
				CSV_EVENT(NavReady, TEXT("CellCommitted_%d"), Cell.Id);
			}
		}
		else if (Cell.State == ENavReadyCellState::Committed || Cell.State == ENavReadyCellState::AIReady)
		{
			Probe.bCellHasDirtyTiles = Generator && Cell.ReadinessBounds.IsValid && Generator->HasDirtyTiles(Cell.ReadinessBounds);
			if (Cell.TrySettle(Probe, Now, Frame))
			{
				// Settled: hash the content the AI is about to use (02 section 2: settle -> hash -> AIReady).
				TArray<UInstancedStaticMeshComponent*> Generated;
				NavReadyPrivate::GetGeneratedComponents(*Component->GetOwner(), Generated);
				Cell.Hash = NavReadyContentHash::HashComponents(Generated);
				QueueBroadcast(Index);
				CSV_EVENT(NavReady, TEXT("CellReady_%d"), Cell.Id);
			}
			else if (Cell.State == ENavReadyCellState::AIReady)
			{
				Cell.ObserveWhileReady(Probe);
				DirtyWhileReady += Cell.bDirtyWhileReady ? 1 : 0;
			}
		}
		++CountByState[static_cast<int32>(Cell.State)];
	}

	CSV_CUSTOM_STAT(NavReady, CellsGenerating, CountByState[static_cast<int32>(ENavReadyCellState::Generating)], ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(NavReady, CellsGenerated, CountByState[static_cast<int32>(ENavReadyCellState::Generated)], ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(NavReady, CellsCommitted, CountByState[static_cast<int32>(ENavReadyCellState::Committed)], ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(NavReady, CellsReady, CountByState[static_cast<int32>(ENavReadyCellState::AIReady)], ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(NavReady, CellsDirtyWhileReady, DirtyWhileReady, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(NavReady, CommitsThisFrame, CommitsThisFrame, ECsvCustomStatOp::Set);

	FlushBroadcasts();
}

TStatId UNavReadySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNavReadySubsystem, STATGROUP_Tickables);
}
