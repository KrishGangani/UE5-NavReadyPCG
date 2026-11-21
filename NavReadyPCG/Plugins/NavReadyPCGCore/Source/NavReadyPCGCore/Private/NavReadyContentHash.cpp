// NavReadyPCGCore - NavReadyContentHash.

#include "NavReadyContentHash.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/Crc.h"

namespace NavReadyContentHash
{
	static int64 Quantize(double Value, double Step)
	{
		return static_cast<int64>(FMath::RoundToDouble(Value / Step));
	}
}

uint32 NavReadyContentHash::HashInstance(FStringView MeshPath, const FTransform& WorldTransform)
{
	const FVector Location = WorldTransform.GetLocation();
	const FQuat Rotation = WorldTransform.GetRotation().GetNormalized();
	const FVector Scale = WorldTransform.GetScale3D();

	// int64 values only, so the array has no padding bytes and its memory can be hashed directly.
	int64 Values[10] = {
		Quantize(Location.X, LocationStep), Quantize(Location.Y, LocationStep), Quantize(Location.Z, LocationStep),
		Quantize(Rotation.X, RotationStep), Quantize(Rotation.Y, RotationStep), Quantize(Rotation.Z, RotationStep), Quantize(Rotation.W, RotationStep),
		Quantize(Scale.X, ScaleStep), Quantize(Scale.Y, ScaleStep), Quantize(Scale.Z, ScaleStep) };

	// q and -q are the same rotation. Pick one representative, after quantization so a component that rounds to 0 can't
	// flip the choice: W positive, or if W is 0, the first non-zero of X, Y, Z positive.
	int64* Quat = &Values[3];
	const int64 Deciding = Quat[3] != 0 ? Quat[3] : (Quat[0] != 0 ? Quat[0] : (Quat[1] != 0 ? Quat[1] : Quat[2]));
	if (Deciding < 0)
	{
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Quat[Index] = -Quat[Index];
		}
	}

	// StrCrc32Len, not StrCrc32: a string view isn't null-terminated, and StrCrc32's second parameter is a seed, not a length.
	const uint32 MeshCrc = FCrc::StrCrc32Len(MeshPath.GetData(), MeshPath.Len());
	return FCrc::MemCrc32(Values, sizeof(Values), MeshCrc);
}

FNavReadyCellHash NavReadyContentHash::Combine(TConstArrayView<uint32> InstanceHashesInOrder)
{
	FNavReadyCellHash Result;
	Result.NumInstances = InstanceHashesInOrder.Num();
	Result.OrderedHash = FCrc::MemCrc32(InstanceHashesInOrder.GetData(), InstanceHashesInOrder.Num() * sizeof(uint32));

	// Sorting the per-instance hashes removes the order and keeps duplicates: the same multiset gives the same hash.
	TArray<uint32> Sorted(InstanceHashesInOrder.GetData(), InstanceHashesInOrder.Num());
	Sorted.Sort();
	Result.ContentHash = FCrc::MemCrc32(Sorted.GetData(), Sorted.Num() * sizeof(uint32));
	return Result;
}

FNavReadyCellHash NavReadyContentHash::HashComponents(TConstArrayView<const UInstancedStaticMeshComponent*> Components)
{
	struct FMeshComponent
	{
		FString MeshPath;
		const UInstancedStaticMeshComponent* Component;
	};
	TArray<FMeshComponent> Ordered;
	for (const UInstancedStaticMeshComponent* Component : Components)
	{
		if (IsValid(Component) && Component->GetStaticMesh())
		{
			Ordered.Add({ Component->GetStaticMesh()->GetPathName(), Component });
		}
	}
	// A fixed component order (PCG's creation order is not part of the content); StableSort keeps ties in input order.
	Ordered.StableSort([](const FMeshComponent& A, const FMeshComponent& B) { return A.MeshPath < B.MeshPath; });

	TArray<uint32> InstanceHashes;
	for (const FMeshComponent& Entry : Ordered)
	{
		const int32 Count = Entry.Component->GetInstanceCount();
		InstanceHashes.Reserve(InstanceHashes.Num() + Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FTransform Transform;
			if (Entry.Component->GetInstanceTransform(Index, Transform, /*bWorldSpace=*/true))
			{
				InstanceHashes.Add(HashInstance(Entry.MeshPath, Transform));
			}
		}
	}
	return Combine(InstanceHashes);
}
