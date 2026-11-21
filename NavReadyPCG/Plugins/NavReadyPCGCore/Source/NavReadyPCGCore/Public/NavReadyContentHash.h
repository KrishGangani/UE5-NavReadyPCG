// NavReadyPCGCore - the determinism check: one hash per cell over everything PCG generated in it.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "Math/Transform.h"

#define UE_API NAVREADYPCGCORE_API

class UInstancedStaticMeshComponent;

/** The hashes of one cell's generated content. */
struct FNavReadyCellHash
{
	/** Order-independent: the same set of instances, in any order, gives the same value. The determinism criterion (SC2). */
	uint32 ContentHash = 0;

	/** Order-dependent: also changes when the same instances come out in a different order (hypothesis H6). Reported only. */
	uint32 OrderedHash = 0;

	int32 NumInstances = 0;

	bool operator==(const FNavReadyCellHash& Other) const = default;
};

/**
 * Plain functions over plain data, so they can be unit-tested without a world (02_System_Architecture.md section 4.1).
 * Transforms are quantized before hashing: float noise smaller than the steps below cannot change a hash, while any real
 * change to a prop's mesh, position, rotation or scale does.
 */
namespace NavReadyContentHash
{
	/** Quantization steps: location 1 mm, quaternion components 1e-4, scale 1e-3 (02_System_Architecture.md section 7). */
	constexpr double LocationStep = 0.1;
	constexpr double RotationStep = 1e-4;
	constexpr double ScaleStep = 1e-3;

	/** Hash of one instance: its mesh's path and its quantized world transform. q and -q (the same rotation) hash the same. */
	UE_API uint32 HashInstance(FStringView MeshPath, const FTransform& WorldTransform);

	/** Combines instance hashes given in generation order into the order-independent and order-dependent cell hashes. */
	UE_API FNavReadyCellHash Combine(TConstArrayView<uint32> InstanceHashesInOrder);

	/** Every instance of the given components (ordered by mesh path, then in instance-index order), combined. */
	UE_API FNavReadyCellHash HashComponents(TConstArrayView<const UInstancedStaticMeshComponent*> Components);
}

#undef UE_API
