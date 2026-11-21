// NavReadyPCGCore - one cell's state and the readiness rule, as plain data (02_System_Architecture.md section 4.1).

#pragma once

#include "CoreMinimal.h"
#include "NavReadyContentHash.h"
#include "NavReadyCell.generated.h"

#define UE_API NAVREADYPCGCORE_API

/** Where a cell is in the pipeline. One vocabulary for code, Blueprint, the CSV and the debug view. */
UENUM(BlueprintType)
enum class ENavReadyCellState : uint8
{
	Idle,       // nothing generated, or cleaned up
	Generating, // PCG is generating the cell
	Generated,  // PCG finished; the content hasn't been handed to navigation yet
	Committed,  // handed to navigation; waiting for its tiles to be rebuilt
	AIReady     // the cell's tiles are rebuilt: AI may use it
};

/** What the navigation system reported this frame, for one cell. Filled by UNavReadySubsystem; made by hand in tests. */
struct FNavReadyProbe
{
	/** A Recast navmesh and its generator exist. Without them nothing can be ready. */
	bool bNavigationAvailable = false;
	/** Any navigation build lock is held (locks are global). */
	bool bBuildLocked = false;
	/** Dirty areas wait to be turned into dirty tiles (global: they could lie in this cell). */
	bool bDirtyAreasQueued = false;
	/** A pending or running tile build overlaps the cell's readiness bounds. */
	bool bCellHasDirtyTiles = false;
};

/**
 * One cell: its state, the area whose tiles decide its readiness, and the moments it changed state. No UObject, so the
 * transitions and the readiness rule can be unit-tested without a world. Times are wall-clock seconds (tile builds and
 * PCG's budget run in real time); frames are GFrameCounter values.
 */
struct FNavReadyCell
{
	ENavReadyCellState State = ENavReadyCellState::Idle;

	/** Stable id, given at registration in registration order (the CSV events use it; registry slots can move). */
	int32 Id = INDEX_NONE;

	/** The cell's content grown by the navmesh's dirty-area border: every tile a change in the cell can mark dirty. */
	FBox ReadinessBounds = FBox(ForceInit);

	double GenerationStartTime = -1.0;
	double GeneratedTime = -1.0;
	double CommitTime = -1.0;
	double ReadyTime = -1.0;
	uint64 CommitFrame = 0;

	/** The content hash, taken when the cell settles, just before it becomes AIReady (02 section 2). */
	FNavReadyCellHash Hash;

	/** Times the cell's tiles became dirty again while it was AIReady (another cell's edge props, for example). */
	int32 TimesDirtiedWhileReady = 0;
	bool bDirtyWhileReady = false;

	/** Any state -> Generating. Clears the timestamps of the previous generation. */
	UE_API void OnGenerationStarted(double Now);

	/** Generating -> Generated. Returns false (and changes nothing) from any other state: a late event after a cancel is ignored. */
	UE_API bool OnGenerated(double Now);

	/** Generated -> Committed. Returns false (and changes nothing) from any other state. */
	UE_API bool OnCommitted(double Now, uint64 Frame);

	/** Committed -> AIReady if the probe shows the cell settled. Returns true on that transition only. */
	UE_API bool TrySettle(const FNavReadyProbe& Probe, double Now, uint64 Frame);

	/** For an AIReady cell: counts each time its tiles turn dirty again. Returns true when they just did. */
	UE_API bool ObserveWhileReady(const FNavReadyProbe& Probe);

	/** Cleaned up or cancelled: -> Idle, timestamps cleared; the id and the bounds are kept. */
	UE_API void OnReset();

	/**
	 * The readiness rule: navigation has seen the commit (at least one frame later: navigation ticks before the tick groups
	 * and registers new content in that tick), no build lock is held, no dirty areas wait anywhere, and no tile build
	 * overlaps the cell.
	 */
	static UE_API bool IsSettled(const FNavReadyProbe& Probe, uint64 CommitFrame, uint64 Frame);
};

#undef UE_API
