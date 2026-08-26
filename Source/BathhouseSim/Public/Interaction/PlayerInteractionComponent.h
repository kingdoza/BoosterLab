#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Interaction/InteractionTypes.h"
#include "PlayerInteractionComponent.generated.h"

class IPlayerInteractable;
class UCameraComponent;
class UPlayerCarryComponent;
class UPlayerEquipmentUseComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionQueryChanged, const FPlayerInteractionQuery&, Query);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInteractionAttemptFinishedNative, const FPlayerInteractionResult&);

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UPlayerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerInteractionComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Configure(UCameraComponent* InCamera, UPlayerCarryComponent* InCarryComponent);
	void ConfigureEquipmentUse(UPlayerEquipmentUseComponent* InEquipmentUseComponent);
	void SetInteractionSuppressed(bool bSuppressed);
	bool IsInteractionSuppressed() const { return bInteractionSuppressed; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FPlayerInteractionQuery GetCurrentInteractionQuery() const { return CurrentQuery; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FPlayerInteractionResult TryInteract();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FPlayerInteractionResult BeginPrimaryInteraction();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void EndPrimaryInteraction();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FPlayerInteractionResult TrySecondaryInteract();

	FPlayerInteractionResult TryDropCarry(const FVector& ViewDirection);
	FPlayerInteractionResult TryDropCarry(const FVector& ViewOrigin, const FVector& ViewDirection);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsPrimaryHoldActive() const { return ActiveHoldTarget != nullptr; }

	void RefreshInteractionQuery();
	void ClearInteractionQuery();
	bool GetCurrentFocusHit(FHitResult& OutHit) const;
	FPlayerInteractionResult ReportExternalInteractionAttempt(const FPlayerInteractionResult& Result);

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionQueryChanged OnInteractionQueryChanged;

	FOnInteractionAttemptFinishedNative OnInteractionAttemptFinishedNative;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "1.0"))
	float TraceDistance = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
	friend class FBathhouseInteractionAttemptResultTest;
	friend class FBathhouseCleaningInteractionTest;
	friend class FBathhouseComputerSessionTest;

	bool BuildInteraction(FPlayerInteractionContext& OutContext, IPlayerInteractable*& OutInteractable, UObject*& OutTargetObject) const;
	bool TraceFocus(FHitResult& OutHit) const;
	FPlayerInteractionResult FinishInteractionAttempt(const FPlayerInteractionResult& Result);
	void TickActiveHold(float DeltaTime);
	void CancelActiveHold(
		bool bBroadcastFailure,
		const FText& FailureReason,
		bool bRefreshQuery = true);
	void ClearActiveHoldState();
	void CommitQuery(UObject* TargetObject, const FPlayerInteractionQuery& NewQuery);

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> CarryComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerEquipmentUseComponent> EquipmentUseComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UObject> CurrentTarget = nullptr;

	UPROPERTY(Transient)
	FPlayerInteractionQuery CurrentQuery;

	UPROPERTY(Transient)
	TObjectPtr<UObject> ActiveHoldTarget = nullptr;

	UPROPERTY(Transient)
	FPlayerInteractionContext ActiveHoldContext;

	float ActiveHoldProgress = 0.0f;
	bool bPrimaryInputHeld = false;
	bool bInteractionSuppressed = false;
};
