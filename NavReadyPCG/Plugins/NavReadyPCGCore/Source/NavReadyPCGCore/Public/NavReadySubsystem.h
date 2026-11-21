// NavReadyPCGCore - the mechanism: the cell registry, the readiness probe, the per-frame tick and the telemetry.

#pragma once

#include "NavReadyCell.h"
#include "Subsystems/WorldSubsystem.h"
#include "NavReadySubsystem.generated.h"

#define UE_API NAVREADYPCGCORE_API

class UNavReadyCellComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FNavReadyAnyCellStateChanged, UNavReadyCellComponent* /*Cell*/, ENavReadyCellState /*NewState*/);

/**
 * Tracks every registered cell from generation to "its navigation tiles are rebuilt" (02_System_Architecture.md
 * sections 4.1 and 6). Game and PIE worlds only.
 *
 * Step S3 (this version) only OBSERVES: a generated cell is marked committed in the next NavReady tick without changing
 * anything (in the baselines its props reached navigation when they were spawned), then the probe decides when its tiles
 * are rebuilt. That is M1 (time to navigable) for every configuration, with the code that will drive NavReady in S5.
 */
UCLASS(MinimalAPI)
class UNavReadySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Every state change of every cell, after the tick that made or saw it. For C++ listeners (baselines, director). */
	FNavReadyAnyCellStateChanged OnAnyCellStateChanged;

	/** The cell's record, or nullptr if it isn't registered. */
	UE_API const FNavReadyCell* FindCell(const UNavReadyCellComponent& Component) const;

	// Called by UNavReadyCellComponent.
	void RegisterCell(UNavReadyCellComponent& Component);
	void UnregisterCell(UNavReadyCellComponent& Component);
	void NotifyGenerationStarted(UNavReadyCellComponent& Component);
	void NotifyGenerated(UNavReadyCellComponent& Component);
	void NotifyReset(UNavReadyCellComponent& Component);

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

protected:
	//~ Begin UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

private:
	/** Index of the component in the registry, or INDEX_NONE. */
	int32 IndexOf(const UNavReadyCellComponent& Component) const;

	/** Records a state change; listeners hear about it when the tick ends. */
	void QueueBroadcast(int32 Index);

	/** Sends the queued state changes. A cell unregistered by a listener only has its slot emptied until the flush ends. */
	void FlushBroadcasts();

	/** Removes emptied slots. */
	void Compact();

	/** The registry: Cells[i] belongs to Components[i]. Plain data in the hot loop; UObjects only where needed. */
	TArray<FNavReadyCell> Cells;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNavReadyCellComponent>> Components;

	/** State changes waiting for the end of the tick. Weak: a listener may destroy a cell before its turn. */
	TArray<TPair<TWeakObjectPtr<UNavReadyCellComponent>, ENavReadyCellState>> PendingBroadcasts;

	bool bBroadcasting = false;

	/** The next cell's id. */
	int32 NextCellId = 0;
};

#undef UE_API
