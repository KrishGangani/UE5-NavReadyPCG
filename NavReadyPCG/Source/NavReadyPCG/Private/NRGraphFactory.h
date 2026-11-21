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
		FVector VoxelSize = FVector(400.0, 400.0, 1000.0);

		/**
		 * Seeded per-point jitter (cm), yaw range (degrees) and uniform scale range, applied by Transform Points. The engine
		 * shapes' pivot is their centre and the points lie on the floor, so a prop stands 50 cm x scale above it: 0.9 keeps
		 * the smallest at 45 cm, above the agent's 35 cm step height. With +-25 cm of jitter on a 4 m lattice, neighbours stay
		 * 3.5 m apart, so even two 1.4 m cubes turned corner to corner leave a 1.5 m gap. A gap closes in the navmesh below
		 * about 1.15 m: the agent's 34 cm radius rounds up to two 19 cm voxels on each side, plus up to a voxel of
		 * rasterization each side. (+-50 cm, a 1 m worst gap, enclosed one sample point in S2's first run: 09 S2.)
		 */
		FVector MaxOffset = FVector(25.0, 25.0, 0.0);
		double MaxYawDegrees = 360.0;
		double MinScale = 0.9;
		double MaxScale = 1.4;

		/** The prop meshes, equally weighted; the spawner picks one per point from the point's seed. */
		TArray<TSoftObjectPtr<UStaticMesh>> Meshes;

		/** The spawner's "Can Ever Affect Navigation" flag: true in the baselines, false for NavReady (02_System_Architecture.md section 9). */
		bool bNavigationVisible = true;
	};

	/**
	 * Input (the cell actor's box) -> Volume Sampler -> Transform Points -> Static Mesh Spawner (collision BlockAll).
	 * The graph is transient and owned by Outer. Returns nullptr (and logs) if the graph could not be wired.
	 */
	UPCGGraph* BuildRockFieldGraph(UObject* Outer, const FRockFieldParams& Params);
}
