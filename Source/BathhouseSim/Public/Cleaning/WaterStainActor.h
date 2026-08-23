#pragma once

#include "CoreMinimal.h"
#include "Cleaning/CleaningTypes.h"
#include "GameFramework/Actor.h"
#include "Interaction/PlayerInteractable.h"
#include "Interaction/HeldEquipmentUsable.h"
#include "WaterStainActor.generated.h"

class AStainSpawnZoneActor;
class UMaterialInterface;
class USceneComponent;
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

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	void SetSpawnZone(AStainSpawnZoneActor* InSpawnZone);
	void ConfigureVisualVariationSeed(int32 InSeed);
	AStainSpawnZoneActor* GetSpawnZone() const { return SpawnZone.Get(); }

	UFUNCTION(BlueprintPure, Category = "Cleaning")
	EStainCleaningState GetCleaningState() const { return CleaningState; }

	UFUNCTION(BlueprintPure, Category = "Cleaning")
	float GetCleaningProgress() const;
	bool QueryMopCleaning(AActor* Cleaner, FText& OutFailureReason, float& OutProgress) const;
	bool BeginMopCleaning(AActor* Cleaner, FText& OutFailureReason);
	FHeldEquipmentUseUpdate UpdateMopCleaning(AActor* Cleaner, float DeltaTime);
	void CancelMopCleaning(AActor* Cleaner);

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnStainCleaningStarted OnCleaningStarted;

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnStainCleaningProgressChanged OnCleaningProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnStainCleaningCancelled OnCleaningCancelled;

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnStainCleaningCompleted OnCleaningCompleted;

	UFUNCTION(BlueprintImplementableEvent, Category = "Cleaning|Presentation")
	void ApplyStainMaterialVariant(UMaterialInterface* SelectedMaterial);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cleaning")
	TObjectPtr<USphereComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cleaning|Presentation")
	TObjectPtr<USceneComponent> StainVisualRoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cleaning|Presentation|Variation")
	TArray<TObjectPtr<UMaterialInterface>> MaterialVariants;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cleaning|Presentation|Variation")
	FVector2D MinXYScale = FVector2D(1.0, 1.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cleaning|Presentation|Variation")
	FVector2D MaxXYScale = FVector2D(1.0, 1.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cleaning|Presentation|Variation")
	float MinYawDegrees = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cleaning|Presentation|Variation")
	float MaxYawDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning", meta = (ClampMin = "0.1"))
	float RemovalDurationSeconds = 2.0f;

private:
	friend class FBathhouseCleaningInteractionTest;

	void ResetCleaning(bool bNotify);
	void CompleteCleaning();
	void ResolveAndApplyVisualVariation();

	UPROPERTY(Transient)
	TObjectPtr<AActor> ActiveCleaner = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SelectedMaterialVariant = nullptr;

	UPROPERTY(Transient)
	FVector2D SelectedXYScale = FVector2D(1.0, 1.0);

	UPROPERTY(Transient)
	float SelectedYawDegrees = 0.0f;

	UPROPERTY(Transient)
	int32 VisualVariationSeed = 0;

	UPROPERTY(Transient)
	bool bHasConfiguredVisualVariationSeed = false;

	UPROPERTY(Transient)
	bool bVisualVariationInitialized = false;

	TWeakObjectPtr<AStainSpawnZoneActor> SpawnZone;
	EStainCleaningState CleaningState = EStainCleaningState::Idle;
	float CleaningElapsedSeconds = 0.0f;
	bool bTerminalCommitted = false;
};
