// P08 NavReadyPCG (workload) - UNRBenchmarkSubsystem.

#include "NRBenchmarkSubsystem.h"

#include "Camera/CameraActor.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "NRCellActor.h"
#include "NRGraphFactory.h"
#include "PCGComponent.h"
#include "PCGWorldActor.h"
#include "Stats/Stats.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NRBenchmarkSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogNRBench, Log, All);

namespace NRBench
{
	/** The arena is built 1 km from the world origin, away from the template map's own floor. Floor top at z = 0. */
	const FVector ArenaCenter(100000.0, 0.0, 0.0);
	/** Half the side of the square arena, in cm (a 200 m x 200 m floor). */
	constexpr double ArenaHalfSize = 10000.0;
	constexpr double FloorThickness = 10.0;
	/** Navigation bounds reach this far above and below the floor, in cm. */
	constexpr double NavBoundsHalfHeight = 500.0;
	/** The engine's 1 m cube: its scale is in units of 100 cm. */
	const TCHAR* const CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	constexpr double CubeSize = 100.0;

	/** S1 test cell: 30 m x 30 m, 1 m tall box standing on the floor, at the arena centre. */
	const FVector TestCellHalfExtent(1500.0, 1500.0, 50.0);
	constexpr double GenerationRadius = 3500.0;

	/** Camera: starts 70 m before the cell (outside its generation radius) and flies towards it at 5 m/s, 15 m up. */
	constexpr double CameraStartOffset = 7000.0;
	constexpr double CameraSpeed = 500.0;
	constexpr double CameraHeight = 1500.0;
	constexpr double CameraPitchDegrees = -45.0;

	/** After generation, navigation must report idle for this many consecutive frames before the carve check reads it
	 *  (one idle frame can be a gap between tile tasks). */
	constexpr int32 RequiredIdleFrames = 2;
	/** Corner-to-corner path test, 5 m inside the arena edges, 50 cm above the floor. */
	constexpr double PathInset = 500.0;
	constexpr double QueryHeight = 50.0;
	/** Carve check: query 10 cm above the floor, 5 cm sideways and 20 cm up/down, so only floor-level navmesh right under the
	 *  point counts (a walkable patch Recast may leave on top of a 1 m prop is out of range). */
	constexpr double CarveQueryHeight = 10.0;
	const FVector CarveQueryExtent(5.0, 5.0, 20.0);
	/** A point half-way between two neighbouring props (the sampler spacing is 3 m). */
	constexpr double BetweenPropsOffset = 150.0;
	/** Frames to wait after the verdict before quitting, so the log is flushed. */
	constexpr int32 ExitDelayFrames = 3;

	const TCHAR* PassText(bool bPassed) { return bPassed ? TEXT("PASS") : TEXT("FAIL"); }

	/** The world's navigation system and its default navigation data; either can be null (then nothing can be queried yet). */
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
	bSpike = FParse::Param(CommandLine, TEXT("NRSpike"));
	bExitWhenDone = FParse::Param(CommandLine, TEXT("NRExit"));
	FParse::Value(CommandLine, TEXT("NRTimeout="), TimeoutSeconds);
	if (!bSpike)
	{
		return; // No workload requested: the world behaves like the plain template map.
	}

	BeginPlayTime = InWorld.GetTimeSeconds();
	if (!BuildArena(InWorld) || !SpawnTestCell(InWorld))
	{
		FinishChecks(0.0); // Nothing can be checked; the summary reports every check as failed.
		return;
	}

	Camera = InWorld.SpawnActor<ACameraActor>();
	UpdateCamera(0.0);
	UE_LOG(LogNRBench, Display, TEXT("NRSpike: arena built; camera starts %.0f cm from the cell, generation radius %.0f cm."),
		CameraDistanceToCell(), NRBench::GenerationRadius);
}

bool UNRBenchmarkSubsystem::SpawnTestCell(UWorld& World)
{
	NRGraphFactory::FRockFieldParams GraphParams;
	GraphParams.Mesh = TSoftObjectPtr<UStaticMesh>(CubeMesh.Get());
	GraphParams.bNavigationVisible = true; // S1 proves the props reach navigation without any coordination.
	Graph = NRGraphFactory::BuildRockFieldGraph(this, GraphParams);
	if (!Graph)
	{
		return false; // BuildRockFieldGraph has logged why.
	}

	// Deferred spawn: the PCG component registers with the PCG subsystem in BeginPlay, so it must be configured first.
	const FVector CellLocation = NRBench::ArenaCenter + FVector(0.0, 0.0, NRBench::TestCellHalfExtent.Z);
	ANRCellActor* Cell = World.SpawnActorDeferred<ANRCellActor>(ANRCellActor::StaticClass(), FTransform(CellLocation),
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Cell)
	{
		UE_LOG(LogNRBench, Error, TEXT("NRSpike: could not spawn the test cell."));
		return false;
	}
	if (!Cell->Configure(Graph, /*Seed=*/1, NRBench::TestCellHalfExtent, NRBench::GenerationRadius))
	{
		Cell->Destroy(); // Never leave a half-spawned (deferred, unfinished) actor in the world.
		return false;
	}
	Cell->GetPCG()->OnPCGGraphStartGeneratingDelegate.AddUObject(this, &UNRBenchmarkSubsystem::HandleCellGenerationStarted);
	Cell->GetPCG()->OnPCGGraphGeneratedDelegate.AddUObject(this, &UNRBenchmarkSubsystem::HandleCellGenerated);
	Cell->FinishSpawning(FTransform(CellLocation));
	TestCell = Cell;
	return true;
}

bool UNRBenchmarkSubsystem::BuildArena(UWorld& World)
{
	CubeMesh = LoadObject<UStaticMesh>(nullptr, NRBench::CubeMeshPath);
	if (!CubeMesh)
	{
		UE_LOG(LogNRBench, Error, TEXT("NRSpike: %s not found (is /Engine/BasicShapes cooked?)."), NRBench::CubeMeshPath);
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Floor: the engine cube, scaled flat, top at z = 0. Movable, because SetStaticMesh refuses to change a Static
	// component once the world has begun play; setting the mesh also registers the floor with navigation.
	const FVector FloorLocation = NRBench::ArenaCenter - FVector(0.0, 0.0, 0.5 * NRBench::FloorThickness);
	AStaticMeshActor* Floor = World.SpawnActor<AStaticMeshActor>(FloorLocation, FRotator::ZeroRotator, SpawnParams);
	if (!Floor)
	{
		UE_LOG(LogNRBench, Error, TEXT("NRSpike: could not spawn the floor."));
		return false;
	}
	Floor->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
	const double FloorScale = 2.0 * NRBench::ArenaHalfSize / NRBench::CubeSize;
	Floor->SetActorScale3D(FVector(FloorScale, FloorScale, NRBench::FloorThickness / NRBench::CubeSize));

	// Navigation bounds. A bounds volume spawned at runtime has no brush geometry (the editor builds that), so its bounds
	// come from an added box component; navigation reads the volume's components bounding box.
	ANavMeshBoundsVolume* Volume = World.SpawnActor<ANavMeshBoundsVolume>(NRBench::ArenaCenter, FRotator::ZeroRotator, SpawnParams);
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
	if (!Volume || !NavSys)
	{
		UE_LOG(LogNRBench, Error, TEXT("NRSpike: navigation bounds volume %s, navigation system %s."),
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
		UE_LOG(LogNRBench, Error, TEXT("NRSpike: could not spawn the PCG world actor."));
		return false;
	}
	return true;
}

void UNRBenchmarkSubsystem::UpdateCamera(double Elapsed)
{
	if (!Camera)
	{
		return;
	}
	const FVector Location = NRBench::ArenaCenter
		+ FVector(-NRBench::CameraStartOffset + NRBench::CameraSpeed * Elapsed, 0.0, NRBench::CameraHeight);
	Camera->SetActorLocationAndRotation(Location, FRotator(NRBench::CameraPitchDegrees, 0.0, 0.0));

	// Re-applied every frame: possessing a pawn would otherwise reset the view target. The PCG generation source is the
	// player controller's view point, so this camera is what drives runtime generation.
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController && PlayerController->GetViewTarget() != Camera)
	{
		PlayerController->SetViewTarget(Camera);
	}
}

double UNRBenchmarkSubsystem::CameraDistanceToCell() const
{
	if (!Camera || !TestCell)
	{
		return -1.0;
	}
	const FBox Box = TestCell->GetCellBounds();
	FVector Point = Camera->GetActorLocation();
	Point.Z = Box.Min.Z;
	return FMath::Sqrt(Box.ComputeSquaredDistanceToPoint(Point));
}

void UNRBenchmarkSubsystem::HandleCellGenerationStarted(UPCGComponent* Component)
{
	GenerationStartDistance = CameraDistanceToCell();
	UE_LOG(LogNRBench, Display, TEXT("NRSpike: runtime generation started, camera %.0f cm from the cell (radius %.0f cm)."),
		GenerationStartDistance, NRBench::GenerationRadius);
}

void UNRBenchmarkSubsystem::HandleCellGenerated(UPCGComponent* Component)
{
	GeneratedInstances = 0;
	TArray<UInstancedStaticMeshComponent*> Generated;
	TestCell->GetGeneratedComponents(Generated);
	for (const UInstancedStaticMeshComponent* ISM : Generated)
	{
		GeneratedInstances += ISM->GetInstanceCount();
	}

	// Pass: generation started inside the radius but not at the very start (the camera began outside it), and produced props.
	const bool bStartedInRange = GenerationStartDistance >= 0.0 && GenerationStartDistance <= NRBench::GenerationRadius;
	const bool bStartedLate = GenerationStartDistance < NRBench::CameraStartOffset - NRBench::TestCellHalfExtent.X;
	GenerationVerdict = (bStartedInRange && bStartedLate && GeneratedInstances > 0) ? EVerdict::Pass : EVerdict::Fail;
	UE_LOG(LogNRBench, Display, TEXT("NRSpike: generation done, %d instances."), GeneratedInstances);
}

bool UNRBenchmarkSubsystem::CheckArenaPath(FString& OutDetail) const
{
	UNavigationSystemV1* NavSys = nullptr;
	const ANavigationData* NavData = NRBench::GetNavigation(GetWorld(), NavSys);
	if (!NavData)
	{
		OutDetail = TEXT("no navigation data");
		return false;
	}
	const double Reach = NRBench::ArenaHalfSize - NRBench::PathInset;
	const FVector Start = NRBench::ArenaCenter + FVector(-Reach, -Reach, NRBench::QueryHeight);
	const FVector End = NRBench::ArenaCenter + FVector(Reach, Reach, NRBench::QueryHeight);

	const FPathFindingResult Result = NavSys->FindPathSync(FPathFindingQuery(this, *NavData, Start, End));
	const bool bComplete = Result.IsSuccessful() && Result.Path.IsValid() && !Result.IsPartial();
	OutDetail = FString::Printf(TEXT("result %d, partial %s, %d points, length %.0f cm (straight line %.0f cm), navdata %s"),
		static_cast<int32>(Result.Result), Result.IsPartial() ? TEXT("yes") : TEXT("no"),
		Result.Path.IsValid() ? Result.Path->GetPathPoints().Num() : 0, Result.Path.IsValid() ? Result.Path->GetLength() : 0.0,
		FVector::Dist(Start, End), *NavData->GetClass()->GetName());
	return bComplete;
}

bool UNRBenchmarkSubsystem::CheckPropCarved(FString& OutDetail) const
{
	UNavigationSystemV1* NavSys = nullptr;
	const ANavigationData* NavData = NRBench::GetNavigation(GetWorld(), NavSys);
	TArray<UInstancedStaticMeshComponent*> Generated;
	TestCell->GetGeneratedComponents(Generated);
	for (const UInstancedStaticMeshComponent* ISM : Generated)
	{
		FTransform Prop;
		if (NavData && ISM->GetInstanceTransform(0, Prop, /*bWorldSpace=*/true))
		{
			const FVector OnProp(Prop.GetLocation().X, Prop.GetLocation().Y, NRBench::CarveQueryHeight);
			const FVector Between = OnProp + FVector(NRBench::BetweenPropsOffset, 0.0, 0.0);
			FNavLocation Ignored;
			const bool bOnPropNavigable = NavSys->ProjectPointToNavigation(OnProp, Ignored, NRBench::CarveQueryExtent, NavData);
			const bool bBetweenNavigable = NavSys->ProjectPointToNavigation(Between, Ignored, NRBench::CarveQueryExtent, NavData);
			OutDetail = FString::Printf(TEXT("navmesh under prop 0: %s; half-way to the next prop: %s; ISM navigation-relevant: %s"),
				bOnPropNavigable ? TEXT("yes") : TEXT("no"), bBetweenNavigable ? TEXT("yes") : TEXT("no"),
				ISM->IsNavigationRelevant() ? TEXT("yes") : TEXT("no"));
			return !bOnPropNavigable && bBetweenNavigable;
		}
	}
	OutDetail = NavData ? TEXT("no generated prop found") : TEXT("no navigation data");
	return false;
}

void UNRBenchmarkSubsystem::TickSpikeChecks(double Elapsed)
{
	// Each check is retried every frame until it passes or the run times out. A single attempt could read navigation in a
	// transient state (the bounds update or a new component's octree registration is deferred to the next navigation tick,
	// 01b_Research_Findings.md #7) and record a false failure. A pass can't be transient: it needs a complete path, or
	// floor navmesh beside a prop and none under it.
	if (NavVerdict == EVerdict::Pending)
	{
		FString Detail;
		if (CheckArenaPath(Detail))
		{
			NavVerdict = EVerdict::Pass;
			UE_LOG(LogNRBench, Display, TEXT("NRSpike: arena path at %.2f s: %s"), Elapsed, *Detail);
		}
	}

	// The carve check also waits until navigation reports idle for a few frames after generation, so it reads rebuilt tiles.
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const bool bNavIdle = NavSys && !NavSys->IsNavigationBuildInProgress() && !NavSys->HasDirtyAreasQueued();
	CarveIdleFrames = (bNavIdle && GenerationVerdict == EVerdict::Pass) ? CarveIdleFrames + 1 : 0;
	if (CarveVerdict == EVerdict::Pending && CarveIdleFrames >= NRBench::RequiredIdleFrames)
	{
		FString Detail;
		if (CheckPropCarved(Detail))
		{
			CarveVerdict = EVerdict::Pass;
			UE_LOG(LogNRBench, Display, TEXT("NRSpike: carve check at %.2f s: %s"), Elapsed, *Detail);
		}
		else
		{
			LastCarveDetail = Detail; // Reported only if the check never passes.
		}
	}

	if (AllChecksPassed() || GenerationVerdict == EVerdict::Fail || Elapsed >= TimeoutSeconds)
	{
		FinishChecks(Elapsed);
	}
}

void UNRBenchmarkSubsystem::FinishChecks(double Elapsed)
{
	// Anything still pending at the end failed.
	for (EVerdict* Verdict : { &NavVerdict, &GenerationVerdict, &CarveVerdict })
	{
		*Verdict = (*Verdict == EVerdict::Pass) ? EVerdict::Pass : EVerdict::Fail;
	}
	UE_LOG(LogNRBench, Display, TEXT("NRSpike result after %.2f s: navmesh %s, runtime generation %s (started %.0f cm from the cell, %d instances), carve %s."),
		Elapsed, NRBench::PassText(NavVerdict == EVerdict::Pass), NRBench::PassText(GenerationVerdict == EVerdict::Pass),
		GenerationStartDistance, GeneratedInstances, NRBench::PassText(CarveVerdict == EVerdict::Pass));
	if (!AllChecksPassed())
	{
		if (CarveVerdict == EVerdict::Fail && !LastCarveDetail.IsEmpty())
		{
			UE_LOG(LogNRBench, Display, TEXT("NRSpike: last carve check: %s"), *LastCarveDetail);
		}
		UE_LOG(LogNRBench, Error, TEXT("NRSpike: FAILED."));
	}
	bChecksDone = true;
}

void UNRBenchmarkSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bSpike)
	{
		return;
	}

	const double Elapsed = GetWorld()->GetTimeSeconds() - BeginPlayTime;
	if (!bChecksDone)
	{
		UpdateCamera(Elapsed);
		TickSpikeChecks(Elapsed);
		return;
	}

	if (bExitWhenDone && ++FramesSinceDone >= NRBench::ExitDelayFrames)
	{
		bExitWhenDone = false; // Request once.
		if (AllChecksPassed())
		{
			FPlatformMisc::RequestExit(false, TEXT("UNRBenchmarkSubsystem"));
		}
		else
		{
			// A non-forced exit drops the return code on Windows (it only posts WM_QUIT), so a failed check would exit 0.
			// The forced exit flushes the log and terminates with code 1, so the queue and scripts see the failure.
			FPlatformMisc::RequestExitWithStatus(true, 1, TEXT("UNRBenchmarkSubsystem"));
		}
	}
}

TStatId UNRBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNRBenchmarkSubsystem, STATGROUP_Tickables);
}
