// NavReadyPCGCore - FNavReadyCell.

#include "NavReadyCell.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavReadyCell)

void FNavReadyCell::OnGenerationStarted(double Now)
{
	OnReset();
	State = ENavReadyCellState::Generating;
	GenerationStartTime = Now;
}

bool FNavReadyCell::OnGenerated(double Now)
{
	if (State != ENavReadyCellState::Generating)
	{
		return false;
	}
	State = ENavReadyCellState::Generated;
	GeneratedTime = Now;
	return true;
}

bool FNavReadyCell::OnCommitted(double Now, uint64 Frame)
{
	if (State != ENavReadyCellState::Generated)
	{
		return false;
	}
	State = ENavReadyCellState::Committed;
	CommitTime = Now;
	CommitFrame = Frame;
	return true;
}

bool FNavReadyCell::TrySettle(const FNavReadyProbe& Probe, double Now, uint64 Frame)
{
	if (State != ENavReadyCellState::Committed || !IsSettled(Probe, CommitFrame, Frame))
	{
		return false;
	}
	State = ENavReadyCellState::AIReady;
	ReadyTime = Now;
	return true;
}

bool FNavReadyCell::ObserveWhileReady(const FNavReadyProbe& Probe)
{
	const bool bWasDirty = bDirtyWhileReady;
	bDirtyWhileReady = State == ENavReadyCellState::AIReady && Probe.bCellHasDirtyTiles;
	if (bDirtyWhileReady && !bWasDirty)
	{
		++TimesDirtiedWhileReady;
		return true;
	}
	return false;
}

void FNavReadyCell::OnReset()
{
	const FBox KeptBounds = ReadinessBounds;
	const int32 KeptId = Id;
	*this = FNavReadyCell();
	ReadinessBounds = KeptBounds;
	Id = KeptId;
}

bool FNavReadyCell::IsSettled(const FNavReadyProbe& Probe, uint64 CommitFrame, uint64 Frame)
{
	return Probe.bNavigationAvailable && Frame > CommitFrame && !Probe.bBuildLocked && !Probe.bDirtyAreasQueued
		&& !Probe.bCellHasDirtyTiles;
}
