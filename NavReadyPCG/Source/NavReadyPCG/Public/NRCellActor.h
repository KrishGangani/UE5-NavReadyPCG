// P08 NavReadyPCG (workload) - one cell: a box that defines the cell's volume, and the PCG component that fills it.

#pragma once

#include "GameFramework/Actor.h"
#include "NRCellActor.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UNavReadyCellComponent;
class UPCGComponent;
class UPCGGraph;

/**
 * A cell of the arena. PCG's runtime-generation scheduler generates its component when a generation source (the player
 * camera) comes within GenerationRadius of the box. The props are spawned onto this actor.
 *
 * Spawn it deferred and call Configure() before FinishSpawning(): the PCG component registers with the PCG subsystem in
 * its BeginPlay, so everything it needs must be set by then.
 */
UCLASS()
class ANRCellActor : public AActor
{
	GENERATED_BODY()

public:
	ANRCellActor();

	/**
	 * Sets the cell's size, graph, seed and generation radius. Call between SpawnActorDeferred and FinishSpawning.
	 * Returns false (and logs) if the radius could not be applied; the cell then must not be used, because it would
	 * generate at once instead of when the camera comes in range.
	 */
	bool Configure(UPCGGraph* Graph, int32 Seed, const FVector& HalfExtent, double GenerationRadius);

	UPCGComponent* GetPCG() const { return PCG; }

	/** NavReady's view of this cell: its state and the moments it changed (S3: observed only). */
	UNavReadyCellComponent* GetNavReady() const { return NavReady; }

	/** The cell's volume in world space (the box, not the generated props). */
	FBox GetCellBounds() const;

	/** The instanced static mesh components PCG generated on this cell (they carry PCG's "PCG Generated Component" tag). */
	void GetGeneratedComponents(TArray<UInstancedStaticMeshComponent*>& OutComponents) const;

private:
	/** The cell volume: the PCG graph samples inside it. No collision, invisible to navigation. */
	UPROPERTY(VisibleAnywhere, Category = "Cell")
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(VisibleAnywhere, Category = "Cell")
	TObjectPtr<UPCGComponent> PCG;

	UPROPERTY(VisibleAnywhere, Category = "Cell")
	TObjectPtr<UNavReadyCellComponent> NavReady;
};
