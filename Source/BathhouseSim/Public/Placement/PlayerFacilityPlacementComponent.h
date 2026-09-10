#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/SupplementalInteractionIntentSource.h"
#include "Placement/FacilityPlacementTypes.h"
#include "PlayerFacilityPlacementComponent.generated.h"

class AFacilityPlacementPreviewActor;
class AFacilityPlacementZoneActor;
class APlaceableFacilityItemActor;
class IPlaceableFacility;
class UCameraComponent;
class UPlayerCarryComponent;
class UPlayerInteractionComponent;

UCLASS(ClassGroup = (Placement), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UPlayerFacilityPlacementComponent
	: public UActorComponent
	, public ISupplementalInteractionIntentSource
{
	GENERATED_BODY()

public:
	UPlayerFacilityPlacementComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Configure(UCameraComponent* InCamera, UPlayerCarryComponent* InCarry, UPlayerInteractionComponent* InInteraction);
	virtual FPlayerInteractionQuery MergeSupplementalInteractionQuery(const FPlayerInteractionQuery& BaseQuery) const override;

	UFUNCTION(BlueprintPure, Category = "Facility Placement")
	bool IsPlacementActive() const { return PreviewFacility.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Facility Placement")
	bool IsRecoveryActive() const { return RecoveryTarget.IsValid(); }

	void SetSnapHeld(bool bHeld);
	void AddRotationInput(float ActionValue);
	FPlayerInteractionResult ConfirmPlacement();
	bool BeginRecoveryHold();
	void UpdateRecoveryHold(float DeltaTime);
	void CompleteRecoveryHold();
	void CancelRecoveryHold();
	void CancelAllSessions();

private:
	friend class FBathhouseFacilityPlacementMathTest;
	friend class FBathhouseFacilityPlacementRuntimeTest;

	UFUNCTION()
	void HandleHeldObjectChanged(AActor* NewHeldObject);
	UFUNCTION()
	void HandlePreviewDestroyed(AActor* DestroyedActor);
	UFUNCTION()
	void HandleRecoveryTargetDestroyed(AActor* DestroyedActor);

	void RefreshPreview();
	void CancelPreview();
	void CancelRecovery();
	void SetPreviewFailure(EFacilityPlacementFailureCode FailureCode, const FText& FailureReason);
	void ClearPreviewVisual();
	void UpdateTickState();
	bool IsLocalSessionOwner() const;
	bool CanProcessSession() const;
	bool TracePlacementZone(AFacilityPlacementZoneActor*& OutZone, FVector& OutPoint) const;
	AActor* TraceRecoveryTarget() const;
	FFacilityPlacementTransactionResult ValidateCurrentPlacement(FTransform& OutCandidate, AFacilityPlacementZoneActor*& OutZone) const;
	FFacilityPlacementTransactionResult ValidateWorldPlacement(
		APlaceableFacilityItemActor& Item,
		const FTransform& Candidate,
		const AFacilityPlacementZoneActor& Zone) const;
	void ReportResult(const FPlayerInteractionResult& Result) const;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> Carry = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerInteractionComponent> Interaction = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlaceableFacilityItemActor> PreviewFacility;

	UPROPERTY(Transient)
	TWeakObjectPtr<AFacilityPlacementZoneActor> PreviewZone;

	UPROPERTY(Transient)
	TWeakObjectPtr<AFacilityPlacementPreviewActor> PreviewActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> RecoveryTarget;

	FFacilityPlacementTransactionResult CurrentPlacementQuery;
	FFacilityPlacementTransactionResult CurrentRecoveryQuery;
	FTransform CurrentCandidate = FTransform::Identity;
	float AccumulatedYaw = 0.0f;
	float RecoveryElapsed = 0.0f;
	bool bSnapHeld = false;
	bool bRecoveryCommittedThisPress = false;
};
