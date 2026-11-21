// NavReadyPCGCore - automation tests for the cell's transitions and the readiness rule, with hand-made probes (no world).
// Run: Tools\run_tests.bat (editor, headless), filter "NavReadyPCG.Cell".

#include "NavReadyCell.h"
#include "Tests/NavReadyTestFlags.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace NavReadyTests
{
	/** A probe that reports nothing in the way: a navmesh exists, no lock, nothing queued, no dirty tile in the cell. */
	FNavReadyProbe ClearProbe()
	{
		FNavReadyProbe Probe;
		Probe.bNavigationAvailable = true;
		return Probe;
	}

	/** A cell committed at frame 10, second 1.0. */
	FNavReadyCell CommittedCell()
	{
		FNavReadyCell Cell;
		Cell.OnGenerationStarted(0.5);
		Cell.OnGenerated(0.9);
		Cell.OnCommitted(1.0, 10);
		return Cell;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavReadyCellCommitFrameTest, "NavReadyPCG.Cell.NotReadyInCommitFrame", NavReadyTests::Flags)
bool FNavReadyCellCommitFrameTest::RunTest(const FString& Parameters)
{
	using namespace NavReadyTests;
	FNavReadyCell Cell = CommittedCell();
	// Navigation registers new content in its next tick, so the commit frame itself can't be settled, however clear.
	TestFalse(TEXT("not ready in the commit frame"), Cell.TrySettle(ClearProbe(), 1.01, 10));
	TestTrue(TEXT("ready one frame later"), Cell.TrySettle(ClearProbe(), 1.02, 11));
	TestTrue(TEXT("state is AIReady"), Cell.State == ENavReadyCellState::AIReady);
	TestEqual(TEXT("ready time recorded"), Cell.ReadyTime, 1.02);
	TestFalse(TEXT("settles only once"), Cell.TrySettle(ClearProbe(), 1.03, 12));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavReadyCellBusyTest, "NavReadyPCG.Cell.NotReadyWhileBusy", NavReadyTests::Flags)
bool FNavReadyCellBusyTest::RunTest(const FString& Parameters)
{
	using namespace NavReadyTests;
	FNavReadyProbe NoNavigation = ClearProbe();
	NoNavigation.bNavigationAvailable = false;
	FNavReadyProbe Locked = ClearProbe();
	Locked.bBuildLocked = true;
	FNavReadyProbe AreasQueued = ClearProbe();
	AreasQueued.bDirtyAreasQueued = true;
	FNavReadyProbe TilesDirty = ClearProbe();
	TilesDirty.bCellHasDirtyTiles = true;

	FNavReadyCell Cell = CommittedCell();
	TestFalse(TEXT("no navmesh"), Cell.TrySettle(NoNavigation, 2.0, 20));
	TestFalse(TEXT("build locked"), Cell.TrySettle(Locked, 2.0, 20));
	TestFalse(TEXT("dirty areas queued"), Cell.TrySettle(AreasQueued, 2.0, 20));
	TestFalse(TEXT("dirty tiles in the cell"), Cell.TrySettle(TilesDirty, 2.0, 20));
	TestTrue(TEXT("still committed"), Cell.State == ENavReadyCellState::Committed);
	TestTrue(TEXT("ready once clear"), Cell.TrySettle(ClearProbe(), 2.5, 21));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavReadyCellTransitionsTest, "NavReadyPCG.Cell.Transitions", NavReadyTests::Flags)
bool FNavReadyCellTransitionsTest::RunTest(const FString& Parameters)
{
	using namespace NavReadyTests;
	FNavReadyCell Cell;
	TestTrue(TEXT("starts idle"), Cell.State == ENavReadyCellState::Idle);
	TestFalse(TEXT("an idle cell can't be committed"), Cell.OnCommitted(0.1, 1));
	TestFalse(TEXT("a generated event without a start is ignored"), Cell.OnGenerated(0.2));
	TestTrue(TEXT("and leaves the cell idle"), Cell.State == ENavReadyCellState::Idle);

	Cell.OnGenerationStarted(0.5);
	TestFalse(TEXT("a generating cell can't be committed"), Cell.OnCommitted(0.6, 2));
	TestFalse(TEXT("a generating cell can't settle"), Cell.TrySettle(ClearProbe(), 0.7, 3));
	Cell.OnGenerated(0.9);
	TestTrue(TEXT("a generated cell commits"), Cell.OnCommitted(1.0, 10));
	TestTrue(TEXT("and settles a frame later"), Cell.TrySettle(ClearProbe(), 1.1, 11));

	// Regeneration starts the pipeline again and forgets the previous generation's times.
	Cell.OnGenerationStarted(5.0);
	TestTrue(TEXT("regenerating"), Cell.State == ENavReadyCellState::Generating);
	TestEqual(TEXT("ready time cleared"), Cell.ReadyTime, -1.0);
	TestEqual(TEXT("new start time"), Cell.GenerationStartTime, 5.0);

	// Cleanup or cancellation: idle, bounds kept (they belong to the cell, not to one generation).
	Cell.ReadinessBounds = FBox(FVector(0.0), FVector(100.0));
	Cell.Id = 7;
	Cell.OnReset();
	TestTrue(TEXT("reset to idle"), Cell.State == ENavReadyCellState::Idle);
	TestTrue(TEXT("bounds kept"), Cell.ReadinessBounds.IsValid != 0);
	TestEqual(TEXT("id kept"), Cell.Id, 7);
	TestFalse(TEXT("a late generated event after a reset is ignored"), Cell.OnGenerated(6.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavReadyCellDirtyWhileReadyTest, "NavReadyPCG.Cell.DirtyWhileReady", NavReadyTests::Flags)
bool FNavReadyCellDirtyWhileReadyTest::RunTest(const FString& Parameters)
{
	using namespace NavReadyTests;
	FNavReadyProbe TilesDirty = ClearProbe();
	TilesDirty.bCellHasDirtyTiles = true;

	FNavReadyCell Cell = CommittedCell();
	TestFalse(TEXT("not counted before ready"), Cell.ObserveWhileReady(TilesDirty));
	Cell.TrySettle(ClearProbe(), 1.1, 11);

	// Counted once per episode: dirty, still dirty, clean, dirty again = 2.
	TestTrue(TEXT("first episode"), Cell.ObserveWhileReady(TilesDirty));
	TestFalse(TEXT("same episode"), Cell.ObserveWhileReady(TilesDirty));
	TestFalse(TEXT("clean"), Cell.ObserveWhileReady(ClearProbe()));
	TestTrue(TEXT("second episode"), Cell.ObserveWhileReady(TilesDirty));
	TestEqual(TEXT("two episodes"), Cell.TimesDirtiedWhileReady, 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
