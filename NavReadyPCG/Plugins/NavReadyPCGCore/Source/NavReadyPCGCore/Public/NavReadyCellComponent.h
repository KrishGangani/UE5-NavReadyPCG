// NavReadyPCGCore - the cell: put it on any actor that owns a UPCGComponent.

#pragma once

#include "Components/ActorComponent.h"
#include "NavReadyCell.h"
#include "NavReadyCellComponent.generated.h"

#define UE_API NAVREADYPCGCORE_API

class UNavReadyCellComponent;
class UPCGComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FNavReadyCellStateChanged, UNavReadyCellComponent*, Cell, ENavReadyCellState, NewState);

/**
 * Follows the owner's PCG component through generation and tells UNavReadySubsystem, which tracks the cell's navigation.
 * A component rather than an actor class, so it attaches to whatever actor a project already uses for its PCG content
 * (02_System_Architecture.md section 4.1). Does nothing in editor worlds (the subsystem only exists in game worlds).
 */
UCLASS(MinimalAPI, ClassGroup = (NavReady), meta = (BlueprintSpawnableComponent))
class UNavReadyCellComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UE_API UNavReadyCellComponent();

	UFUNCTION(BlueprintCallable, Category = "NavReady")
	UE_API ENavReadyCellState GetState() const;

	UFUNCTION(BlueprintCallable, Category = "NavReady")
	UE_API bool IsAIReady() const;

	/** The cell's record (state, readiness bounds, timestamps); nullptr while not registered. */
	UE_API const FNavReadyCell* GetCell() const;

	/** Broadcast after every state change, at the end of the NavReady tick that made or saw it. */
	UPROPERTY(BlueprintAssignable, Category = "NavReady")
	FNavReadyCellStateChanged OnCellStateChanged;

protected:
	//~ Begin UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent

private:
	void HandleGenerationStarted(UPCGComponent* Component);
	void HandleGenerated(UPCGComponent* Component);
	void HandleReset(UPCGComponent* Component);

	/** The owner's PCG component, found in BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<UPCGComponent> PCG;
};

#undef UE_API
