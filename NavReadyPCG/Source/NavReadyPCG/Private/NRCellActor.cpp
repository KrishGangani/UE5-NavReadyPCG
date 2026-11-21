// P08 NavReadyPCG (workload) - ANRCellActor.

#include "NRCellActor.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Helpers/PCGHelpers.h"
#include "NavReadyCellComponent.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "RuntimeGen/SchedulingPolicies/PCGSchedulingPolicyDistanceAndDirection.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NRCellActor)

DEFINE_LOG_CATEGORY_STATIC(LogNRCell, Log, All);

namespace NRCell
{
	/** Cleanup happens beyond GenerationRadius x this scalar. 100 keeps every cell of the arena generated for a whole run. */
	constexpr float NeverCleanUpScalar = 100.0f;

	/**
	 * Writes one FPerQualityLevelFloat member of the component's FPCGRuntimeGenerationRadii. Those members are protected in
	 * C++ (PCGCommon.h) but BlueprintReadWrite, so they are written through reflection: the same path a Blueprint uses.
	 */
	bool SetRadiiMember(UPCGComponent& Component, FName Member, float Value)
	{
		const FStructProperty* RadiiProperty = CastField<FStructProperty>(
			UPCGComponent::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UPCGComponent, GenerationRadii)));
		const FStructProperty* MemberProperty = RadiiProperty ? CastField<FStructProperty>(RadiiProperty->Struct->FindPropertyByName(Member)) : nullptr;
		// Compared by name: FPerQualityLevelFloat is not exported from the Engine module, so its StaticStruct() can't be linked.
		static const FName PerQualityLevelFloatName(TEXT("PerQualityLevelFloat"));
		if (!MemberProperty || MemberProperty->Struct->GetFName() != PerQualityLevelFloatName)
		{
			UE_LOG(LogNRCell, Error, TEXT("FPCGRuntimeGenerationRadii has no FPerQualityLevelFloat member '%s' in this engine version."), *Member.ToString());
			return false;
		}
		*MemberProperty->ContainerPtrToValuePtr<FPerQualityLevelFloat>(&Component.GenerationRadii) = FPerQualityLevelFloat(Value);
		return true;
	}
}

ANRCellActor::ANRCellActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Bounds->SetGenerateOverlapEvents(false);
	Bounds->SetCanEverAffectNavigation(false); // Shape components can affect navigation by default (ShapeComponent.cpp:61).
	RootComponent = Bounds;

	PCG = CreateDefaultSubobject<UPCGComponent>(TEXT("PCG"));
	PCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateAtRuntime;

	// Follows the PCG component and registers the cell with NavReady (it finds the PCG component in its BeginPlay).
	NavReady = CreateDefaultSubobject<UNavReadyCellComponent>(TEXT("NavReady"));
}

bool ANRCellActor::Configure(UPCGGraph* Graph, int32 Seed, const FVector& HalfExtent, double GenerationRadius)
{
	Bounds->SetBoxExtent(HalfExtent, /*bUpdateOverlaps=*/false);

	PCG->Seed = Seed;

	// Non-partitioned runtime components generate when a generation source is within this radius of the component's
	// bounds. Without the override the radius comes from the graph, whose default is effectively unlimited.
	const bool bRadiiSet = NRCell::SetRadiiMember(*PCG, TEXT("RadiusUnbounded"), static_cast<float>(GenerationRadius))
		&& NRCell::SetRadiiMember(*PCG, TEXT("CleanupRadiusScalar"), NRCell::NeverCleanUpScalar);
	PCG->bOverrideGenerationRadii = bRadiiSet;
	PCG->GenerationRadii.ComputeHash(); // "Call this anytime the values are changed" (PCGCommon.h).

	// The constructor only sets the policy class; the scheduler needs the policy object, which this creates.
	PCG->SetSchedulingPolicyClass(UPCGSchedulingPolicyDistanceAndDirection::StaticClass());

	PCG->SetGraphLocal(Graph);
	return bRadiiSet;
}

FBox ANRCellActor::GetCellBounds() const
{
	return Bounds->Bounds.GetBox();
}

void ANRCellActor::GetGeneratedComponents(TArray<UInstancedStaticMeshComponent*>& OutComponents) const
{
	GetComponents(OutComponents);
	OutComponents.RemoveAll([](const UInstancedStaticMeshComponent* Component)
	{
		return !Component->ComponentHasTag(PCGHelpers::DefaultPCGTag);
	});
}
