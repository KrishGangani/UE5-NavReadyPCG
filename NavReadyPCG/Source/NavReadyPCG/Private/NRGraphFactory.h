// P08 NavReadyPCG (workload) - builds the one PCG graph every cell uses, in code, so the workload is reviewable as text.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"

class UObject;
class UPCGGraph;
class UStaticMesh;

namespace NRGraphFactory
{
	/** What the graph spawns. One value per run: every cell shares the graph; only the component seed differs per cell. */
	struct FRockFieldParams
	{
		/** Distance between sampled points inside the cell's box, in cm. Z is larger than the box height: one layer. */
		FVector VoxelSize = FVector(300.0, 300.0, 1000.0);

		/** The prop mesh. */
		TSoftObjectPtr<UStaticMesh> Mesh;

		/** The spawner's "Can Ever Affect Navigation" flag: true in the baselines, false for NavReady (02_System_Architecture.md section 9). */
		bool bNavigationVisible = true;
	};

	/**
	 * Input (the cell actor's box) -> Volume Sampler -> Static Mesh Spawner (collision BlockAll).
	 * The graph is transient and owned by Outer. Returns nullptr (and logs) if the graph could not be wired.
	 */
	UPCGGraph* BuildRockFieldGraph(UObject* Outer, const FRockFieldParams& Params);
}
