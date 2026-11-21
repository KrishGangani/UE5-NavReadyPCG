// P08 NavReadyPCG (workload) - benchmark director: builds the arena at runtime and runs one repeatable workload.

#pragma once

#include "NavReadyContentHash.h"
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
 * Step S2 (this version): builds the arena and a 4 x 4 grid of cells (3 x 3 navmesh tiles each), waits until the arena
 * is navigable and checks that the tiles match the cells, then flies the camera over the grid (a cruise, then a burst)
 * so PCG's runtime scheduler generates the cells. Every cell's generation times and content hash go into a run report.
 * The S1 spike checks were removed: their results are in 09.
 *
 * Step S3: the run is also a measurement. NavReady (the plugin) observes every cell until its tiles are rebuilt; the
 * report gains the commit and ready times, and the CSV capture runs from BenchStart (the flight) to BenchEnd.
 *
 * Command line (all optional):
 *   -NRRun          build and run the workload (without it the world is the plain template map)
 *   -NRSeed=N       base seed, 0 to 1000000 (default 1); cell i gets seed N * 100 + i
 *   -NRRunTag=NAME  run report file Saved/NRRuns/<NAME>.csv (default "run"; characters not allowed in a file name become _)
 *   -NRTimeout=SEC  give up after this many seconds of world time (default 90)
 *   -NRExit         quit when the run is finished; exit code 1 if the run failed (FinishRun says why)
 *   -NRConfig=ID    B0 (default: uncoordinated) or NR (NavReady: props spawned navigation-invisible, committed per cell)
 *   -NRCells=K      spawn only the first K cells, 1 to 16 (default 16); K = 1 isolates one cell's navigation work
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
	/** What happened to one cell. Times are wall-clock seconds since the flight started (tile builds and PCG's budget run in real time). */
	struct FCellRecord
	{
		FIntPoint Grid = FIntPoint::ZeroValue;
		int32 Seed = 0;
		double GenerationStart = -1.0;
		double GenerationDone = -1.0;
		FNavReadyCellHash Hash;
		/** NavReady's observations, copied when the run ends: seconds since the flight started (-1 = never). */
		double CommitTime = -1.0;
		double ReadyTime = -1.0;
		int32 DirtiedWhileReady = 0;
		bool bHashMatchesAtReady = false;
		/** Pocket check, on the settled navmesh: sample points inside the cell that lie on the navmesh, and how many of them no path reaches. */
		int32 NavPoints = 0;
		int32 PocketPoints = 0;
	};

	enum class EPhase : uint8
	{
		Idle,              // no workload requested
		WaitingForNavmesh, // warm-up: the camera holds still until a path crosses the whole arena
		Flying,            // the camera flies its path; cells generate as it comes within range
		Settling,          // every cell generated: wait until navigation is idle, then look for enclosed pockets
		Done
	};

	/** Spawns the floor, the navigation bounds and the PCG world actor. Returns false (and logs) if something is missing. */
	bool BuildArena(UWorld& World);

	/** Builds the graph and spawns the configured 4 x 4 cells. Returns false (and logs); never leaves a half-spawned cell. */
	bool SpawnCells(UWorld& World);

	/** True once the navmesh returns a complete path corner to corner across the arena. */
	bool IsArenaNavigable() const;

	/** True if the navmesh's tiles are the ones the cell grid was laid out on (NRBench::NavTileSize from the world origin); logs if not. */
	bool AreTilesAlignedWithCells() const;

	/** True when navigation has no dirty areas queued and no tile build in progress. */
	bool IsNavigationIdle() const;

	/** Fills every record's pocket counts: navmesh points inside the cell that no path from the arena corner reaches. */
	void CheckPockets();

	/** Places the camera on its path, FlightSeconds after the flight started (0 before it starts). */
	void UpdateCamera(double FlightSeconds);

	void HandleCellGenerationStarted(UPCGComponent* Component, int32 CellIndex);
	void HandleCellGenerated(UPCGComponent* Component, int32 CellIndex);

	/** Logs every cell and the run summary, writes the run report, and moves to Done. The run passes if bCompleted and every cell generated. */
	void FinishRun(const TCHAR* Reason, bool bCompleted);

	/** Saved/NRRuns/<RunTag>.csv: one line per cell. */
	void WriteRunReport() const;

	int32 CellsGenerated() const;

	/** Cells NavReady reports AIReady (in S3: their tiles are rebuilt). */
	int32 CellsReady() const;

	// Settings read once from the command line.
	bool bExitWhenDone = false;
	int32 BaseSeed = 1;
	FString RunTag = TEXT("run");
	FString Config = TEXT("B0");
	int32 CellCount = 16;
	double TimeoutSeconds = 90.0;

	EPhase Phase = EPhase::Idle;
	double BeginPlayTime = 0.0;
	double FlightStartWorldTime = 0.0;
	double FlightStartWallTime = 0.0;
	double SettleStartWorldTime = 0.0;
	int32 IdleFrames = 0;
	bool bRunPassed = false;
	/** A cell generated while the camera was not flying (the warm-up position must be out of every cell's range). */
	bool bGeneratedOutsideFlight = false;
	int32 FramesSinceDone = 0;

	TArray<FCellRecord> Records;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ANRCellActor>> Cells;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> PropMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UPCGGraph> Graph;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
};
