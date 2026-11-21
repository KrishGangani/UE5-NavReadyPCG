// P08 NavReadyPCG (workload) - NRGraphFactory.

#include "NRGraphFactory.h"

#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGTransformPoints.h"
#include "Elements/PCGVolumeSampler.h"
#include "Engine/CollisionProfile.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "PCGCommon.h"
#include "PCGGraph.h"
#include "PCGNode.h"

DEFINE_LOG_CATEGORY_STATIC(LogNRGraph, Log, All);

UPCGGraph* NRGraphFactory::BuildRockFieldGraph(UObject* Outer, const FRockFieldParams& Params)
{
	if (Params.Meshes.IsEmpty())
	{
		UE_LOG(LogNRGraph, Error, TEXT("The rock-field graph needs at least one mesh."));
		return nullptr;
	}

	UPCGGraph* Graph = NewObject<UPCGGraph>(Outer, NAME_None, RF_Transient);

	// Points on a regular grid inside the input volume (the cell actor's box). The Create Points Grid node would be the
	// obvious choice, but its settings class is not exported from the PCG module, so it can't be created from our code.
	UPCGVolumeSamplerSettings* Sampler = nullptr;
	UPCGNode* SamplerNode = Graph->AddNodeOfType<UPCGVolumeSamplerSettings>(Sampler);
	Sampler->VoxelSize = Params.VoxelSize;

	// Seeded jitter, yaw and uniform scale per point: deterministic for a given seed, different between cells and seeds.
	UPCGTransformPointsSettings* Transform = nullptr;
	UPCGNode* TransformNode = Graph->AddNodeOfType<UPCGTransformPointsSettings>(Transform);
	Transform->OffsetMin = -Params.MaxOffset;
	Transform->OffsetMax = Params.MaxOffset;
	Transform->RotationMin = FRotator::ZeroRotator;
	Transform->RotationMax = FRotator(0.0, Params.MaxYawDegrees, 0.0);
	Transform->ScaleMin = FVector(Params.MinScale);
	Transform->ScaleMax = FVector(Params.MaxScale);
	Transform->bUniformScale = true;

	// One instanced static mesh per point, with collision that blocks Pawn: that is what makes the props matter to navigation.
	UPCGStaticMeshSpawnerSettings* Spawner = nullptr;
	UPCGNode* SpawnerNode = Graph->AddNodeOfType<UPCGStaticMeshSpawnerSettings>(Spawner);
	UPCGMeshSelectorWeighted* Selector = CastChecked<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters);
	for (const TSoftObjectPtr<UStaticMesh>& Mesh : Params.Meshes)
	{
		FPCGMeshSelectorWeightedEntry& Entry = Selector->MeshEntries.Emplace_GetRef(Mesh, /*InWeight=*/1);
		Entry.Descriptor.BodyInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Entry.Descriptor.bCanEverAffectNavigation = Params.bNavigationVisible;
	}

	// The graph input node's output pin "In" carries the owning actor's data: for our cell actor, its box as a collision-shape
	// volume. (Not PCGInputOutputConstants::DefaultInputLabel, "Input": the input node's static pin is PCGPinConstants' "In".)
	Graph->AddEdge(Graph->GetInputNode(), PCGPinConstants::DefaultInputLabel, SamplerNode, PCGVolumeSamplerConstants::VolumeLabel);
	Graph->AddEdge(SamplerNode, PCGPinConstants::DefaultOutputLabel, TransformNode, PCGPinConstants::DefaultInputLabel);
	Graph->AddEdge(TransformNode, PCGPinConstants::DefaultOutputLabel, SpawnerNode, PCGPinConstants::DefaultInputLabel);

	// Neither AddEdge (always returns the target node) nor AddLabeledEdge (returns "did the target pin drop other edges")
	// reports success, so the wiring is checked on the target pins themselves.
	const bool bWired = SamplerNode->IsInputPinConnected(PCGVolumeSamplerConstants::VolumeLabel)
		&& TransformNode->IsInputPinConnected(PCGPinConstants::DefaultInputLabel)
		&& SpawnerNode->IsInputPinConnected(PCGPinConstants::DefaultInputLabel);
	if (!bWired)
	{
		UE_LOG(LogNRGraph, Error, TEXT("Could not wire the rock-field graph; no content will be generated."));
		return nullptr;
	}
	return Graph;
}
