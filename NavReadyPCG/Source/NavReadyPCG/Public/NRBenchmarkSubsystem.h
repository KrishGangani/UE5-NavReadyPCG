// P08 NavReadyPCG (workload) - benchmark director: builds the arena at runtime and runs one repeatable workload.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "NRBenchmarkSubsystem.generated.h"

class ACameraActor;
class ANRCellActor;
class UPCGComponent;
class UPCGGraph;
class UStaticMesh;

/**
 * Runs the workload for one game world. A world subsystem instead of a placed actor: the project has no authored map,
 * so nothing would place a director, and the subsystem already has exactly the world's lifetime. Tickable, because it
 * drives the camera (the PCG generation source) every frame.
 *
 * Step S1 (this version) builds the arena and runs the world spike: does a world built entirely at runtime, in a
 * packaged build, get a navmesh and run PCG's runtime-generation scheduler? See 03_Implementation_Roadmap.md, S1.
 *
 * Command line (all optional):
 *   -NRSpike         build the arena and run the S1 checks (without it the world is the plain template map)
 *   -NRTimeout=SEC   give up on the checks after this many seconds of world time (default 60)
 *   -NRExit          quit when the checks are finished
 */
UCLASS()
class UNRBenchmarkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

protected:
	//~ Begin UWorldSubsystem
	/** Game and PIE worlds only. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

private:
	/** Spawns the floor, the navigation bounds and the PCG world actor. Returns false (and logs) if something is missing. */
	bool BuildArena(UWorld& World);

	/** Builds the graph and spawns the configured test cell. Returns false (and logs); never leaves a half-spawned cell. */
	bool SpawnTestCell(UWorld& World);

	/** Moves the camera along the spike path: from outside the test cell's generation radius, straight towards it. */
	void UpdateCamera(double Elapsed);

	/** One frame of the S1 checks. Each check is retried until it passes; the run ends when all pass, or at the timeout. */
	void TickSpikeChecks(double Elapsed);

	/** Turns pending verdicts into failures, logs the one-line summary (always, on every path) and sets bChecksDone. */
	void FinishChecks(double Elapsed);

	bool AllChecksPassed() const
	{
		return NavVerdict == EVerdict::Pass && GenerationVerdict == EVerdict::Pass && CarveVerdict == EVerdict::Pass;
	}

	/** A path across the whole arena, corner to corner. True if the navmesh returned a complete path. */
	bool CheckArenaPath(FString& OutDetail) const;

	/** True if the first generated prop is carved out of the navmesh while the space between props is not. */
	bool CheckPropCarved(FString& OutDetail) const;

	void HandleCellGenerationStarted(UPCGComponent* Component);
	void HandleCellGenerated(UPCGComponent* Component);

	/** 2D distance from the camera to the test cell's box, in cm (the scheduler's 2D grid ignores height). */
	double CameraDistanceToCell() const;

	enum class EVerdict : uint8 { Pending, Pass, Fail };

	bool bSpike = false;
	bool bExitWhenDone = false;
	double TimeoutSeconds = 60.0;
	double BeginPlayTime = 0.0;

	EVerdict NavVerdict = EVerdict::Pending;
	EVerdict GenerationVerdict = EVerdict::Pending;
	EVerdict CarveVerdict = EVerdict::Pending;
	int32 CarveIdleFrames = 0;
	FString LastCarveDetail;
	double GenerationStartDistance = -1.0;
	int32 GeneratedInstances = 0;
	bool bChecksDone = false;
	int32 FramesSinceDone = 0;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UPCGGraph> Graph;

	UPROPERTY(Transient)
	TObjectPtr<ANRCellActor> TestCell;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
};
