#pragma once

#include "CoreMinimal.h"
#include "Cleaning/CleaningTypes.h"
#include "GameFramework/Actor.h"
#include "Interaction/PlayerInteractable.h"
#include "WaterStainActor.generated.h"

class AStainSpawnZoneActor;
class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStainCleaningStarted, AActor*, Cleaner);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStainCleaningProgressChanged, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStainCleaningCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStainCleaningCompleted);

UCLASS(Blueprintable)
class BATHHOUSESIM_API AWaterStainActor : public AActor, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	AWaterStainActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual bool BeginHoldInteraction(const FPlayerInteractionContext& Context, FText& OutFailureReason) override;
	virtual FPlayerHoldInteractionUpdate UpdateHoldInteraction(
		const FPlayerInteractionContext& Context,
		float DeltaTime) override;
	virtual void CancelHoldInteraction(const FPlayerInteractionContext& Context) override;

	void SetSpawnZone(AStainSpawnZoneActor* InSpawnZone);
	AStainSpawnZoneActor* GetSpawnZone() const { return SpawnZone.Get(); }

	UFUNCTION(BlueprintPure, Category = "Cleaning")
	EStainCleaningState GetCleaningState() const { return CleaningState; }

	UFUNCTION(BlueprintPure, Category = "Cleaning")
	float GetCleaningProgress() const;

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnStainCleaningStarted OnCleaningStarted;

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnStainCleaningProgressChanged OnCleaningProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnStainCleaningCancelled OnCleaningCancelled;

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnStainCleaningCompleted OnCleaningCompleted;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cleaning")
	TObjectPtr<USphereComponent> InteractionCollision;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning", meta = (ClampMin = "0.1"))
	float RemovalDurationSeconds = 2.0f;

private:
	bool HasRequiredMop(const FPlayerInteractionContext& Context) const;
	void ResetCleaning(bool bNotify);
	void CompleteCleaning();

	UPROPERTY(Transient)
	TObjectPtr<AActor> ActiveCleaner = nullptr;

	TWeakObjectPtr<AStainSpawnZoneActor> SpawnZone;
	EStainCleaningState CleaningState = EStainCleaningState::Idle;
	float CleaningElapsedSeconds = 0.0f;
	bool bTerminalCommitted = false;
};
