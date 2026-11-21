// P08 NavReadyPCG (workload) - UNRBenchmarkSubsystem.

#include "NRBenchmarkSubsystem.h"

#include "Algo/Count.h"
#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "NavReadyCellComponent.h"
#include "NRCellActor.h"
#include "NRGraphFactory.h"
#include "PCGComponent.h"
#include "PCGWorldActor.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/Stats.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NRBenchmarkSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogNRBench, Log, All);

// BenchStart / BenchEnd open and close the CSV capture (-csvStartOnEvent / -csvStopOnEvent match the event text).
CSV_DEFINE_CATEGORY(NRBench, true);

namespace NRBench
{
	/**
	 * The navmesh tile side with the engine defaults: TileSizeUU 1000 truncated to whole 19 cm voxels, 52 x 19 = 988 cm
	 * (ARecastNavMesh::GetTileSizeUU). Tiles start at the world origin. Checked against the real navmesh at warm-up.
	 */
	constexpr double NavTileSize = 988.0;
	/**
	 * The arena is built about 1 km from the world origin (away from the template map's floor), on a tile corner, and one
	 * tile off the X axis so that no cell edge lies on the Volume Sampler's 4 m lattice (see SpawnCells). Floor top at z = 0.
	 */
	const FVector ArenaCenter(101.0 * NavTileSize, NavTileSize, 0.0);
	/** Half the side of the square arena, in cm (a 200 m x 200 m floor). */
	constexpr double ArenaHalfSize = 10000.0;
	constexpr double FloorThickness = 10.0;
	/** Navigation bounds reach this far above and below the floor, in cm. */
	constexpr double NavBoundsHalfHeight = 500.0;
	/** The engine's 1 m shapes (scale is in units of 100 cm). The first is also the floor. */
	const TCHAR* const PropMeshPaths[] = { TEXT("/Engine/BasicShapes/Cube.Cube"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder") };
	constexpr double ShapeSize = 100.0;

	/**
	 * The cells: a 4 x 4 grid of 1 m tall boxes standing on the floor, centred in the arena. Each cell is exactly 3 x 3
	 * navmesh tiles (29.64 m), so no tile lies in two cells (only a prop at a cell's edge can dirty a neighbour's tile).
	 */
	constexpr int32 GridSide = 4;
	constexpr double CellSize = 3.0 * NavTileSize;
	constexpr double CellHeight = 100.0;
	constexpr double GenerationRadius = 3500.0;
	/** Cell seeds: BaseSeed * 100 + cell index, so a different base seed changes every cell. */
	constexpr int32 SeedStride = 100;

	/**
	 * The camera path, along the arena's X axis through its centre, 15 m up, looking ahead and down: a cruise (one grid
	 * column comes into range at a time), then a burst (the last two columns come into range within about 4 s).
	 */
	constexpr double PathStartX = -11000.0;
	constexpr double BurstStartX = -5000.0;
	constexpr double PathEndX = 11000.0;
	constexpr double CruiseSpeed = 400.0;
	constexpr double BurstSpeed = 1200.0;
	constexpr double CameraHeight = 1500.0;
	constexpr double CameraPitchDegrees = -45.0;

	/** Warm-up gate: a corner-to-corner path, 5 m inside the arena edges, 50 cm above the floor. */
	constexpr double PathInset = 500.0;
	constexpr double QueryHeight = 50.0;
	/**
	 * Pocket check: a 14 x 14 grid of points per cell (about 2.1 m apart), each projected onto the navmesh from 10 cm above
	 * the floor within a box of +-5 cm horizontally and +-20 cm vertically. That box finds the floor's navmesh but not the
	 * tops of props (45 cm or taller, NRGraphFactory::FRockFieldParams), and fails where a prop stands; a point that
	 * projects but can't be reached is a pocket.
	 */
	constexpr int32 PocketSamplesPerSide = 14;
	constexpr double PocketQueryHeight = 10.0;
	const FVector PocketQueryExtent(5.0, 5.0, 20.0);
	/**
	 * Settling: at least this long (world time) after the last cell generated, so its props have reached navigation, and
	 * then this many idle navigation frames in a row (one idle frame can be a gap between tile tasks).
	 */
	constexpr double SettleSeconds = 1.0;
	constexpr int32 RequiredIdleFrames = 2;
	/** Command-line limits: the seed so that BaseSeed * SeedStride + index stays far inside int32. */
	constexpr int32 MaxBaseSeed = 1000000;
	constexpr int32 ExitDelayFrames = 3;

	/** Camera X along the path, FlightSeconds after the flight started. */
	double CameraX(double FlightSeconds)
	{
		const double CruiseSeconds = (BurstStartX - PathStartX) / CruiseSpeed;
		if (FlightSeconds <= CruiseSeconds)
		{
			return PathStartX + CruiseSpeed * FMath::Max(0.0, FlightSeconds);
		}
		return FMath::Min(PathEndX, BurstStartX + BurstSpeed * (FlightSeconds - CruiseSeconds));
	}

	/** A corner of the arena, PathInset inside its edges and QueryHeight above the floor: -1 the near corner, +1 the far one. */
	FVector ArenaCorner(double Sign)
	{
		const double Reach = Sign * (ArenaHalfSize - PathInset);
		return ArenaCenter + FVector(Reach, Reach, QueryHeight);
	}

	/** The world's default navigation data, or nullptr. OutNavSys is the navigation system, or nullptr. */
	const ANavigationData* GetNavigation(UWorld* World, UNavigationSystemV1*& OutNavSys)
	{
		OutNavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		return OutNavSys ? OutNavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
	}
}

bool UNRBenchmarkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UNRBenchmarkSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const TCHAR* CommandLine = FCommandLine::Get();
	if (!FParse::Param(CommandLine, TEXT("NRRun")))
	{
		return; // No workload requested: the world behaves like the plain template map.
	}
	bExitWhenDone = FParse::Param(CommandLine, TEXT("NRExit"));
	FParse::Value(CommandLine, TEXT("NRSeed="), BaseSeed);
	FParse::Value(CommandLine, TEXT("NRRunTag="), RunTag);
	FParse::Value(CommandLine, TEXT("NRTimeout="), TimeoutSeconds);
	FParse::Value(CommandLine, TEXT("NRConfig="), Config);
	FParse::Value(CommandLine, TEXT("NRCells="), CellCount);
	CellCount = FMath::Clamp(CellCount, 1, NRBench::GridSide * NRBench::GridSide);
	BaseSeed = FMath::Clamp(BaseSeed, 0, NRBench::MaxBaseSeed);
	RunTag = FPaths::MakeValidFileName(RunTag, TEXT('_')); // the tag names the report file: no folders, no '..'

	BeginPlayTime = InWorld.GetTimeSeconds();
	if (Config != TEXT("B0") && Config != TEXT("NR"))
	{
		UE_LOG(LogNRBench, Error, TEXT("NRRun: unknown configuration '%s' (B0 or NR)."), *Config);
		FinishRun(TEXT("unknown configuration"), /*bCompleted=*/false);
		return;
	}
	if (!BuildArena(InWorld) || !SpawnCells(InWorld))
	{
		FinishRun(TEXT("set-up failed"), /*bCompleted=*/false);
		return;
	}

	Camera = InWorld.SpawnActor<ACameraActor>();
	UpdateCamera(0.0);
	Phase = EPhase::WaitingForNavmesh;
	UE_LOG(LogNRBench, Display, TEXT("NRRun '%s' (%s): %d cells, base seed %d; waiting for the navmesh."), *RunTag, *Config, Cells.Num(), BaseSeed);
}

bool UNRBenchmarkSubsystem::BuildArena(UWorld& World)
{
	for (const TCHAR* Path : NRBench::PropMeshPaths)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path);
		if (!Mesh)
		{
			UE_LOG(LogNRBench, Error, TEXT("NRRun: %s not found (is /Engine/BasicShapes cooked?)."), Path);
			return false;
		}
		PropMeshes.Add(Mesh);
		const FBox Bounds = Mesh->GetBoundingBox();
		UE_LOG(LogNRBench, Display, TEXT("NRRun: prop mesh %s, bounds z %.1f to %.1f cm around its pivot."), Path, Bounds.Min.Z, Bounds.Max.Z);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Floor: the engine cube, scaled flat, top at z = 0. Movable, because SetStaticMesh refuses to change a Static
	// component once the world has begun play; setting the mesh also registers the floor with navigation.
	const FVector FloorLocation = NRBench::ArenaCenter - FVector(0.0, 0.0, 0.5 * NRBench::FloorThickness);
	AStaticMeshActor* Floor = World.SpawnActor<AStaticMeshActor>(FloorLocation, FRotator::ZeroRotator, SpawnParams);
	if (!Floor)
	{
		UE_LOG(LogNRBench, Error, TEXT("NRRun: could not spawn the floor."));
		return false;
	}
	Floor->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(PropMeshes[0]);
	const double FloorScale = 2.0 * NRBench::ArenaHalfSize / NRBench::ShapeSize;
	Floor->SetActorScale3D(FVector(FloorScale, FloorScale, NRBench::FloorThickness / NRBench::ShapeSize));

	// Navigation bounds. A bounds volume spawned at runtime has no brush geometry (the editor builds that), so its bounds
	// come from an added box component; navigation reads the volume's components bounding box.
	ANavMeshBoundsVolume* Volume = World.SpawnActor<ANavMeshBoundsVolume>(NRBench::ArenaCenter, FRotator::ZeroRotator, SpawnParams);
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
	if (!Volume || !NavSys)
	{
		UE_LOG(LogNRBench, Error, TEXT("NRRun: navigation bounds volume %s, navigation system %s."),
			Volume ? TEXT("ok") : TEXT("missing"), NavSys ? TEXT("ok") : TEXT("missing"));
		return false;
	}
	UBoxComponent* Extent = NewObject<UBoxComponent>(Volume, TEXT("ArenaExtent"));
	Extent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Extent->SetCanEverAffectNavigation(false);
	Extent->SetBoxExtent(FVector(NRBench::ArenaHalfSize, NRBench::ArenaHalfSize, NRBench::NavBoundsHalfHeight), /*bUpdateOverlaps=*/false);
	Extent->SetupAttachment(Volume->GetRootComponent());
	Extent->RegisterComponent();
	NavSys->OnNavigationBoundsUpdated(Volume);

	// Runtime generation only ticks when a PCG world actor is registered, and nothing creates one at runtime
	// (01b_Research_Findings.md #8). It registers itself in BeginPlay.
	if (!World.SpawnActor<APCGWorldActor>(APCGWorldActor::StaticClass(), FTransform::Identity, SpawnParams))
	{
		UE_LOG(LogNRBench, Error, TEXT("NRRun: could not spawn the PCG world actor."));
		return false;
	}
	return true;
}

bool UNRBenchmarkSubsystem::SpawnCells(UWorld& World)
{
	NRGraphFactory::FRockFieldParams GraphParams;
	for (UStaticMesh* Mesh : PropMeshes)
	{
		GraphParams.Meshes.Add(TSoftObjectPtr<UStaticMesh>(Mesh));
	}
	// B0: props reach navigation the moment PCG spawns them. NR: spawned navigation-invisible; NavReady commits each
	// generated cell as a whole (02 section 9). The flag is not part of the content hash.
	GraphParams.bNavigationVisible = Config != TEXT("NR");
	Graph = NRGraphFactory::BuildRockFieldGraph(this, GraphParams);
	if (!Graph)
	{
		return false; // BuildRockFieldGraph has logged why.
	}

	// The Volume Sampler samples a world-aligned lattice (multiples of VoxelSize), so a cell edge exactly on it would give
	// the same point to both cells. The arena's placement avoids that; checked here in case a constant changes.
	const double GridMin = -0.5 * NRBench::GridSide * NRBench::CellSize;
	for (int32 Edge = 0; Edge <= NRBench::GridSide; ++Edge)
	{
		const FVector EdgePoint = NRBench::ArenaCenter + FVector(GridMin + Edge * NRBench::CellSize);
		for (int32 Axis = 0; Axis < 2; ++Axis)
		{
			const double Remainder = FMath::Abs(FMath::Fmod(EdgePoint[Axis], GraphParams.VoxelSize[Axis]));
			if (FMath::Min(Remainder, GraphParams.VoxelSize[Axis] - Remainder) < 1.0)
			{
				UE_LOG(LogNRBench, Error, TEXT("NRRun: a cell edge (%.0f cm on axis %d) lies on the %.0f cm sampling lattice."),
					EdgePoint[Axis], Axis, GraphParams.VoxelSize[Axis]);
				return false;
			}
		}
	}

	const FVector HalfExtent(0.5 * NRBench::CellSize, 0.5 * NRBench::CellSize, 0.5 * NRBench::CellHeight);
	const double GridOffset = 0.5 * (NRBench::GridSide - 1);
	for (int32 Row = 0; Row < NRBench::GridSide; ++Row)
	{
		for (int32 Column = 0; Column < NRBench::GridSide && Records.Num() < CellCount; ++Column)
		{
			const int32 Index = Records.Num();
			const FVector Location = NRBench::ArenaCenter
				+ FVector((Column - GridOffset) * NRBench::CellSize, (Row - GridOffset) * NRBench::CellSize, HalfExtent.Z);
			const int32 Seed = BaseSeed * NRBench::SeedStride + Index;

			// Deferred spawn: the PCG component registers with the PCG subsystem in BeginPlay, so it must be configured first.
			ANRCellActor* Cell = World.SpawnActorDeferred<ANRCellActor>(ANRCellActor::StaticClass(), FTransform(Location),
				nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Cell)
			{
				UE_LOG(LogNRBench, Error, TEXT("NRRun: could not spawn cell %d."), Index);
				return false;
			}
			if (!Cell->Configure(Graph, Seed, HalfExtent, NRBench::GenerationRadius))
			{
				Cell->Destroy(); // Never leave a half-spawned (deferred, unfinished) actor in the world.
				return false;
			}
			// The record and the cell exist before the delegates are bound, so a handler can never index past them.
			FCellRecord& Record = Records.AddDefaulted_GetRef();
			Record.Grid = FIntPoint(Column, Row);
			Record.Seed = Seed;
			Cells.Add(Cell);
			Cell->GetPCG()->OnPCGGraphStartGeneratingDelegate.AddUObject(this, &UNRBenchmarkSubsystem::HandleCellGenerationStarted, Index);
			Cell->GetPCG()->OnPCGGraphGeneratedDelegate.AddUObject(this, &UNRBenchmarkSubsystem::HandleCellGenerated, Index);
			Cell->FinishSpawning(FTransform(Location));
		}
	}
	return true;
}

bool UNRBenchmarkSubsystem::IsArenaNavigable() const
{
	UNavigationSystemV1* NavSys = nullptr;
	const ANavigationData* NavData = NRBench::GetNavigation(GetWorld(), NavSys);
	if (!NavData)
	{
		return false;
	}
	const FPathFindingResult Result = NavSys->FindPathSync(FPathFindingQuery(this, *NavData, NRBench::ArenaCorner(-1.0), NRBench::ArenaCorner(1.0)));
	return Result.IsSuccessful() && !Result.IsPartial();
}

bool UNRBenchmarkSubsystem::AreTilesAlignedWithCells() const
{
	UNavigationSystemV1* NavSys = nullptr;
	const ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NRBench::GetNavigation(GetWorld(), NavSys));
	if (!NavMesh)
	{
		UE_LOG(LogNRBench, Error, TEXT("NRRun: the default navigation data is not a Recast navmesh."));
		return false;
	}
	const double TileSize = NavMesh->GetTileSizeUU();
	const bool bAligned = FMath::IsNearlyEqual(TileSize, NRBench::NavTileSize, 0.01) && NavMesh->NavMeshOriginOffset.IsNearlyZero();
	if (!bAligned)
	{
		UE_LOG(LogNRBench, Error, TEXT("NRRun: the cells assume %.0f cm navmesh tiles starting at the world origin; the navmesh has %.2f cm tiles, origin offset %s."),
			NRBench::NavTileSize, TileSize, *NavMesh->NavMeshOriginOffset.ToString());
	}
	return bAligned;
}

bool UNRBenchmarkSubsystem::IsNavigationIdle() const
{
	UNavigationSystemV1* NavSys = nullptr;
	return NRBench::GetNavigation(GetWorld(), NavSys) && !NavSys->HasDirtyAreasQueued() && !NavSys->IsNavigationBuildInProgress();
}

void UNRBenchmarkSubsystem::CheckPockets()
{
	UNavigationSystemV1* NavSys = nullptr;
	const ANavigationData* NavData = NRBench::GetNavigation(GetWorld(), NavSys);
	if (!NavData)
	{
		return;
	}
	const FVector Start = NRBench::ArenaCorner(-1.0);
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		FCellRecord& Record = Records[Index];
		const FBox Bounds = Cells[Index]->GetCellBounds();
		const FVector Step = Bounds.GetSize() / NRBench::PocketSamplesPerSide;
		for (int32 X = 0; X < NRBench::PocketSamplesPerSide; ++X)
		{
			for (int32 Y = 0; Y < NRBench::PocketSamplesPerSide; ++Y)
			{
				const FVector Point(Bounds.Min.X + (X + 0.5) * Step.X, Bounds.Min.Y + (Y + 0.5) * Step.Y,
					NRBench::ArenaCenter.Z + NRBench::PocketQueryHeight);
				FNavLocation OnNavmesh;
				if (!NavSys->ProjectPointToNavigation(Point, OnNavmesh, NRBench::PocketQueryExtent, NavData))
				{
					continue; // a prop stands here
				}
				++Record.NavPoints;
				const FPathFindingResult Result = NavSys->FindPathSync(FPathFindingQuery(this, *NavData, Start, OnNavmesh.Location));
				if (!Result.IsSuccessful() || Result.IsPartial())
				{
					++Record.PocketPoints;
					UE_LOG(LogNRBench, Display, TEXT("NRRun: pocket point in cell %d at (%.0f, %.0f) cm: no complete path reaches it."),
						Index, OnNavmesh.Location.X, OnNavmesh.Location.Y);
				}
			}
		}
	}
}

void UNRBenchmarkSubsystem::UpdateCamera(double FlightSeconds)
{
	if (!Camera)
	{
		return;
	}
	const FVector Location = NRBench::ArenaCenter + FVector(NRBench::CameraX(FlightSeconds), 0.0, NRBench::CameraHeight);
	Camera->SetActorLocationAndRotation(Location, FRotator(NRBench::CameraPitchDegrees, 0.0, 0.0));

	// Re-applied every frame: possessing a pawn would otherwise reset the view target. The PCG generation source is the
	// player controller's view point, so this camera is what drives runtime generation.
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController && PlayerController->GetViewTarget() != Camera)
	{
		PlayerController->SetViewTarget(Camera);
	}
}

void UNRBenchmarkSubsystem::HandleCellGenerationStarted(UPCGComponent* Component, int32 CellIndex)
{
	if (Phase != EPhase::Flying && Phase != EPhase::Settling)
	{
		// Times are measured from the flight's start, so they would be meaningless; and the workload would differ.
		UE_LOG(LogNRBench, Error, TEXT("NRRun: cell %d started generating before the camera's flight."), CellIndex);
		bGeneratedOutsideFlight = true;
	}
	Records[CellIndex].GenerationStart = FPlatformTime::Seconds() - FlightStartWallTime;
}

void UNRBenchmarkSubsystem::HandleCellGenerated(UPCGComponent* Component, int32 CellIndex)
{
	FCellRecord& Record = Records[CellIndex];
	Record.GenerationDone = FPlatformTime::Seconds() - FlightStartWallTime;

	TArray<UInstancedStaticMeshComponent*> Generated;
	Cells[CellIndex]->GetGeneratedComponents(Generated);
	Record.Hash = NavReadyContentHash::HashComponents(Generated);
}

int32 UNRBenchmarkSubsystem::CellsReady() const
{
	return Algo::CountIf(Cells, [](const ANRCellActor* Cell) { return IsValid(Cell) && Cell->GetNavReady()->IsAIReady(); });
}

int32 UNRBenchmarkSubsystem::CellsGenerated() const
{
	return Algo::CountIf(Records, [](const FCellRecord& Record) { return Record.GenerationDone >= 0.0; });
}

void UNRBenchmarkSubsystem::FinishRun(const TCHAR* Reason, bool bCompleted)
{
	const bool bFlew = FlightStartWallTime > 0.0;
	if (bFlew)
	{
		CSV_EVENT(NRBench, TEXT("BenchEnd")); // closes the capture before the reporting below
	}
	Phase = EPhase::Done;
	FramesSinceDone = 0;

	// NavReady's observations, relative to the flight's start like the generation times.
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		const FNavReadyCell* Observed = IsValid(Cells[Index]) ? Cells[Index]->GetNavReady()->GetCell() : nullptr;
		if (Observed && bFlew)
		{
			FCellRecord& Record = Records[Index];
			Record.CommitTime = Observed->CommitTime >= 0.0 ? Observed->CommitTime - FlightStartWallTime : -1.0;
			Record.ReadyTime = Observed->ReadyTime >= 0.0 ? Observed->ReadyTime - FlightStartWallTime : -1.0;
			Record.DirtiedWhileReady = Observed->TimesDirtiedWhileReady;
			Record.bHashMatchesAtReady = Observed->Hash == Record.Hash;
		}
	}

	int32 Instances = 0;
	int32 PocketPoints = 0;
	uint32 RunHash = 0;
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		const FCellRecord& Record = Records[Index];
		Instances += Record.Hash.NumInstances;
		PocketPoints += Record.PocketPoints;
		RunHash = FCrc::TypeCrc32(Record.Hash.ContentHash, RunHash); // chained in cell order: one value to compare runs by
		UE_LOG(LogNRBench, Display, TEXT("NRRun cell %2d (%d,%d) seed %d: %3d instances, content 0x%08X, ordered 0x%08X, generation %.3f -> %.3f s, ready %.3f s, pockets %d of %d navmesh points"),
			Index, Record.Grid.X, Record.Grid.Y, Record.Seed, Record.Hash.NumInstances, Record.Hash.ContentHash,
			Record.Hash.OrderedHash, Record.GenerationStart, Record.GenerationDone, Record.ReadyTime, Record.PocketPoints, Record.NavPoints);
	}

	// Every cell generated, has props, and at least one sample point in it is blocked: the props reached navigation.
	// (Without the last two, a run whose props lost their collision or navigation flag would still pass.)
	const int32 Generated = CellsGenerated();
	const int32 SamplesPerCell = NRBench::PocketSamplesPerSide * NRBench::PocketSamplesPerSide;
	const int32 CellsWithBlockedPoints = Algo::CountIf(Records, [SamplesPerCell](const FCellRecord& Record)
		{ return Record.Hash.NumInstances > 0 && Record.NavPoints < SamplesPerCell; });
	const int32 Ready = Algo::CountIf(Records, [](const FCellRecord& Record) { return Record.ReadyTime >= 0.0; });
	bRunPassed = bCompleted && !bGeneratedOutsideFlight && Records.Num() > 0 && Generated == Records.Num()
		&& CellsWithBlockedPoints == Records.Num() && Ready == Records.Num();
	UE_LOG(LogNRBench, Display, TEXT("NRRun '%s' result (%s): %d/%d cells generated, %d instances, %d pocket points, run content hash 0x%08X."),
		*RunTag, Reason, Generated, Records.Num(), Instances, PocketPoints, RunHash);
	// NavReady hashes each cell when it settles; it must be the content the director hashed at generation.
	const int32 HashMatches = Algo::CountIf(Records, [](const FCellRecord& Record) { return Record.bHashMatchesAtReady; });
	UE_LOG(LogNRBench, Display, TEXT("NRRun: %d/%d cells have props that block navigation; %d/%d reached navigation-ready; NavReady's hash at ready matches the generated content in %d/%d cells."),
		CellsWithBlockedPoints, Records.Num(), Ready, Records.Num(), HashMatches, Records.Num());
	if (!bRunPassed)
	{
		UE_LOG(LogNRBench, Error, TEXT("NRRun: FAILED."));
	}
	WriteRunReport();
}

void UNRBenchmarkSubsystem::WriteRunReport() const
{
	FString Report = FString::Printf(TEXT("# run=%s config=%s base_seed=%d cells=%d generation_radius_cm=%.0f\n"),
		*RunTag, *Config, BaseSeed, Records.Num(), NRBench::GenerationRadius);
	Report += TEXT("cell,grid_x,grid_y,seed,instances,content_hash,ordered_hash,generation_start_s,generation_done_s,committed_s,ready_s,dirtied_while_ready,nav_points,pocket_points\n");
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		const FCellRecord& Record = Records[Index];
		Report += FString::Printf(TEXT("%d,%d,%d,%d,%d,0x%08X,0x%08X,%.6f,%.6f,%.6f,%.6f,%d,%d,%d\n"), Index, Record.Grid.X, Record.Grid.Y,
			Record.Seed, Record.Hash.NumInstances, Record.Hash.ContentHash, Record.Hash.OrderedHash,
			Record.GenerationStart, Record.GenerationDone, Record.CommitTime, Record.ReadyTime, Record.DirtiedWhileReady,
			Record.NavPoints, Record.PocketPoints);
	}

	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NRRuns"), RunTag + TEXT(".csv"));
	if (FFileHelper::SaveStringToFile(Report, *Path))
	{
		UE_LOG(LogNRBench, Display, TEXT("Run report written: %s"), *FPaths::ConvertRelativePathToFull(Path));
	}
	else
	{
		UE_LOG(LogNRBench, Error, TEXT("Could not write the run report: %s"), *Path);
	}
}

void UNRBenchmarkSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UWorld* World = GetWorld();
	const double Elapsed = World->GetTimeSeconds() - BeginPlayTime;
	switch (Phase)
	{
	case EPhase::WaitingForNavmesh:
		// Navigable, and then idle for RequiredIdleFrames frames: the arena's first build is finished before any cell
		// starts, so no cell's time-to-navigable includes it.
		IdleFrames = IsNavigationIdle() ? IdleFrames + 1 : 0;
		if (IsArenaNavigable() && IdleFrames >= NRBench::RequiredIdleFrames)
		{
			if (!AreTilesAlignedWithCells())
			{
				FinishRun(TEXT("the navmesh tiles don't match the cells"), /*bCompleted=*/false);
				break;
			}
			Phase = EPhase::Flying;
			FlightStartWorldTime = World->GetTimeSeconds();
			FlightStartWallTime = FPlatformTime::Seconds();
			CSV_METADATA(TEXT("NRRunTag"), *RunTag);
			CSV_METADATA(TEXT("NRConfig"), *Config);
			CSV_METADATA(TEXT("NRSeed"), *FString::FromInt(BaseSeed));
			CSV_EVENT(NRBench, TEXT("BenchStart"));
			UE_LOG(LogNRBench, Display, TEXT("NRRun: navmesh ready after %.2f s (%.0f cm tiles, aligned with the cells); the camera starts its path."),
				Elapsed, NRBench::NavTileSize);
		}
		else if (Elapsed >= TimeoutSeconds)
		{
			FinishRun(TEXT("the navmesh never became navigable"), /*bCompleted=*/false);
		}
		break;

	case EPhase::Flying:
		UpdateCamera(World->GetTimeSeconds() - FlightStartWorldTime);
		if (CellsGenerated() == Records.Num())
		{
			Phase = EPhase::Settling;
			SettleStartWorldTime = World->GetTimeSeconds();
			IdleFrames = 0;
		}
		else if (Elapsed >= TimeoutSeconds)
		{
			FinishRun(TEXT("timeout"), /*bCompleted=*/false);
		}
		break;

	case EPhase::Settling:
		// The last cell's props reach the navigation octree in a later navigation tick, so wait a moment before trusting
		// "idle"; the pocket check needs the settled navmesh.
		UpdateCamera(World->GetTimeSeconds() - FlightStartWorldTime);
		IdleFrames = IsNavigationIdle() ? IdleFrames + 1 : 0;
		if (World->GetTimeSeconds() - SettleStartWorldTime >= NRBench::SettleSeconds && IdleFrames >= NRBench::RequiredIdleFrames
			&& CellsReady() == Records.Num())
		{
			const double CheckStart = FPlatformTime::Seconds();
			CheckPockets();
			UE_LOG(LogNRBench, Display, TEXT("NRRun: navigation settled %.2f s after the last cell; the pocket check took %.2f s."),
				World->GetTimeSeconds() - SettleStartWorldTime, FPlatformTime::Seconds() - CheckStart);
			FinishRun(TEXT("all cells generated"), /*bCompleted=*/true);
		}
		else if (Elapsed >= TimeoutSeconds)
		{
			FinishRun(TEXT("timeout: navigation never settled"), /*bCompleted=*/false);
		}
		break;

	case EPhase::Done:
		if (bExitWhenDone && ++FramesSinceDone >= NRBench::ExitDelayFrames)
		{
			bExitWhenDone = false; // Request once.
			if (bRunPassed)
			{
				FPlatformMisc::RequestExit(false, TEXT("UNRBenchmarkSubsystem"));
			}
			else
			{
				// A non-forced exit drops the return code on Windows (it only posts WM_QUIT), so a failed run would exit 0.
				// The forced exit flushes the log and terminates with code 1, so the queue and scripts see the failure.
				FPlatformMisc::RequestExitWithStatus(true, 1, TEXT("UNRBenchmarkSubsystem"));
			}
		}
		break;

	default:
		break;
	}
}

TStatId UNRBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNRBenchmarkSubsystem, STATGROUP_Tickables);
}
