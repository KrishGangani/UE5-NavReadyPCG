// NavReadyPCGCore - UNavReadyCellComponent.

#include "NavReadyCellComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavReadySubsystem.h"
#include "PCGComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavReadyCellComponent)

DEFINE_LOG_CATEGORY_STATIC(LogNavReadyCell, Log, All);

UNavReadyCellComponent::UNavReadyCellComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // the subsystem does the per-frame work for every cell at once
}

ENavReadyCellState UNavReadyCellComponent::GetState() const
{
	const FNavReadyCell* Cell = GetCell();
	return Cell ? Cell->State : ENavReadyCellState::Idle;
}

bool UNavReadyCellComponent::IsAIReady() const
{
	return GetState() == ENavReadyCellState::AIReady;
}

const FNavReadyCell* UNavReadyCellComponent::GetCell() const
{
	const UWorld* World = GetWorld();
	const UNavReadySubsystem* Subsystem = World ? World->GetSubsystem<UNavReadySubsystem>() : nullptr;
	return Subsystem ? Subsystem->FindCell(*this) : nullptr;
}

void UNavReadyCellComponent::BeginPlay()
{
	Super::BeginPlay();

	UNavReadySubsystem* Subsystem = GetWorld()->GetSubsystem<UNavReadySubsystem>();
	if (!Subsystem)
	{
		return; // not a game world
	}
	PCG = GetOwner()->FindComponentByClass<UPCGComponent>();
	if (!PCG)
	{
		UE_LOG(LogNavReadyCell, Error, TEXT("%s has a NavReady cell component but no PCG component; the cell is ignored."), *GetNameSafe(GetOwner()));
		return;
	}
	// Native delegates: PCG fires them for runtime generation too (01b_Research_Findings.md #9).
	PCG->OnPCGGraphStartGeneratingDelegate.AddUObject(this, &UNavReadyCellComponent::HandleGenerationStarted);
	PCG->OnPCGGraphGeneratedDelegate.AddUObject(this, &UNavReadyCellComponent::HandleGenerated);
	PCG->OnPCGGraphCancelledDelegate.AddUObject(this, &UNavReadyCellComponent::HandleReset);
	PCG->OnPCGGraphCleanedDelegate.AddUObject(this, &UNavReadyCellComponent::HandleReset);
	Subsystem->RegisterCell(*this);
}

void UNavReadyCellComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PCG)
	{
		PCG->OnPCGGraphStartGeneratingDelegate.RemoveAll(this);
		PCG->OnPCGGraphGeneratedDelegate.RemoveAll(this);
		PCG->OnPCGGraphCancelledDelegate.RemoveAll(this);
		PCG->OnPCGGraphCleanedDelegate.RemoveAll(this);
	}
	if (UNavReadySubsystem* Subsystem = GetWorld()->GetSubsystem<UNavReadySubsystem>())
	{
		Subsystem->UnregisterCell(*this);
	}
	Super::EndPlay(EndPlayReason);
}

void UNavReadyCellComponent::HandleGenerationStarted(UPCGComponent* Component)
{
	if (UNavReadySubsystem* Subsystem = GetWorld()->GetSubsystem<UNavReadySubsystem>())
	{
		Subsystem->NotifyGenerationStarted(*this);
	}
}

void UNavReadyCellComponent::HandleGenerated(UPCGComponent* Component)
{
	if (UNavReadySubsystem* Subsystem = GetWorld()->GetSubsystem<UNavReadySubsystem>())
	{
		Subsystem->NotifyGenerated(*this);
	}
}

void UNavReadyCellComponent::HandleReset(UPCGComponent* Component)
{
	if (UNavReadySubsystem* Subsystem = GetWorld()->GetSubsystem<UNavReadySubsystem>())
	{
		Subsystem->NotifyReset(*this);
	}
}
