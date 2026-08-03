#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Customer/BathhouseCustomerTypes.h"
#include "Engine/EngineTypes.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "Interaction/InteractionTypes.h"
#include "CustomerSessionComponent.generated.h"

class ABathhouseCashPaymentActor;
class ABathhouseCounterActor;
class ABathhouseFacilityActor;
class ABathhouseKeyActor;
class UBathhouseFacilitySlotComponent;
class UCustomerRoutineDefinition;

UCLASS(ClassGroup = (Customer), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UCustomerSessionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomerSessionComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeSession(UCustomerRoutineDefinition* InRoutineDefinition, ABathhouseCounterActor* InCounter);

	FPlayerInteractionQuery QueryCheckInInteraction(const FPlayerInteractionContext& Context) const;
	FPlayerInteractionResult ExecuteCheckInInteraction(const FPlayerInteractionContext& Context);

	bool JoinQueue(EBathhouseCounterLane Lane);
	void LeaveQueue();
	bool IsQueueFront() const;
	bool GetQueueTargetTransform(FTransform& OutTransform) const;
	void BeginWaitingForCheckIn();
	void CancelCheckInWait();

	bool TryReserveFacility(EBathhouseFacilityType FacilityType, bool bExcludeLastBath = false);
	bool BeginUseCurrentFacility();
	bool SnapCurrentFacility(ECustomerFacilitySnapTarget Target);
	void ReleaseCurrentFacility();
	bool GetCurrentFacilityTransform(bool bApproach, FTransform& OutTransform) const;
	void WaitForFacility(EBathhouseFacilityType FacilityType);
	void StopWaitingForFacility();

	float BeginActivity(EBathhouseCustomerActivity Activity);
	void FinishActivity(EBathhouseCustomerActivity Activity);
	void AbortActivity(EBathhouseCustomerActivity Activity, bool bReleaseFacility = true);

	bool StartBathStay();
	bool IsBathStayExpired() const { return bBathStayExpired; }
	float GetRemainingBathStaySeconds() const;

	bool TryPlaceCheckoutKey();
	bool TryCreateCashOffer(TSubclassOf<ABathhouseCashPaymentActor> CashClass);
	bool BeginCheckoutOffer();
	void EndCheckoutOffer();
	void CancelCashOffer();
	bool IsCashClaimed() const { return bCashClaimed; }

	bool RegisterNavigationFailure();
	void ResetNavigationFailures() { NavigationFailureCount = 0; }
	void FinishSession(EBathhouseCustomerDepartureReason Reason);
	void TechnicalAbort(const FString& ErrorMessage);

	UFUNCTION(BlueprintPure, Category = "Customer Session")
	UCustomerRoutineDefinition* GetRoutineDefinition() const { return RoutineDefinition; }

	UFUNCTION(BlueprintPure, Category = "Customer Session")
	ABathhouseCounterActor* GetCounter() const { return Counter; }

	UFUNCTION(BlueprintPure, Category = "Customer Session")
	ABathhouseKeyActor* GetAssignedKey() const { return AssignedKey; }

	UFUNCTION(BlueprintPure, Category = "Customer Session")
	int32 GetKeyNumber() const { return KeyNumber; }

	UFUNCTION(BlueprintPure, Category = "Customer Session")
	EBathhouseCustomerActivity GetCurrentActivity() const { return CurrentActivity; }

	UFUNCTION(BlueprintPure, Category = "Customer Session")
	EBathhouseCustomerDepartureReason GetDepartureReason() const { return DepartureReason; }

	bool HasAssignedKey() const { return AssignedKey != nullptr; }
	bool DidCheckInTimeOut() const { return bCheckInTimedOut; }
	bool IsWaitingForCheckIn() const { return bWaitingForCheckIn; }
	bool HasCurrentFacility() const { return CurrentFacilitySlot != nullptr; }
	bool IsSnappedToFacilityActionPoint() const { return bSnappedToFacilityActionPoint; }
	bool IsFinished() const { return bFinished; }
	bool IsTechnicalAbort() const { return DepartureReason == EBathhouseCustomerDepartureReason::TechnicalAbort; }
	EBathhouseCounterLane GetQueueLane() const { return QueueLane; }

private:
	friend class FBathhouseQueueCleanupTest;
	friend class FBathhouseCustomerBathSnapTest;

	void CacheCurrentFacilityTransforms();
	void ClearCurrentFacilityTransformCache();
	bool SnapToCurrentFacilityActionPoint();
	bool ReturnToCurrentFacilityApproachPoint();
	bool IsActionTransformClear(const class ACharacter& Character) const;
	void RestoreSavedMovementMode(class ACharacter& Character);
	void SendCustomerEvent(const FGameplayTag& EventTag) const;
	bool ShouldForwardQueueChangedEvent(EBathhouseCounterLane ChangedLane) const;
	void SetPresentationState(EBathhouseCustomerPresentationState NewState) const;
	void HandleCheckInTimeout();
	void HandleBathStayExpired();
	void HandleFacilityAvailabilityChanged(EBathhouseFacilityType FacilityType);
	void HandleQueueChanged(EBathhouseCounterLane ChangedLane);

	UFUNCTION()
	void HandleCashClaimed(ABathhouseCashPaymentActor* CashActor);

	UPROPERTY(Transient)
	TObjectPtr<UCustomerRoutineDefinition> RoutineDefinition = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABathhouseCounterActor> Counter = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABathhouseKeyActor> AssignedKey = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABathhouseFacilityActor> CurrentFacilityActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UBathhouseFacilitySlotComponent> CurrentFacilitySlot = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABathhouseFacilityActor> LastBathActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABathhouseCashPaymentActor> CashOffer = nullptr;

	int32 KeyNumber = INDEX_NONE;
	int32 ReturnSlotIndex = INDEX_NONE;
	int32 NavigationFailureCount = 0;
	EBathhouseCounterLane QueueLane = EBathhouseCounterLane::None;
	EBathhouseFacilityType WaitingFacilityType = EBathhouseFacilityType::Bath;
	EBathhouseCustomerActivity CurrentActivity = EBathhouseCustomerActivity::None;
	EBathhouseCustomerDepartureReason DepartureReason = EBathhouseCustomerDepartureReason::None;
	double BathStayEndTime = 0.0;
	FTimerHandle CheckInTimeoutHandle;
	FTimerHandle BathStayTimerHandle;
	FDelegateHandle FacilityAvailabilityHandle;
	FDelegateHandle QueueChangedHandle;
	FTransform CachedFacilityApproachTransform;
	FTransform CachedFacilityActionTransform;
	TEnumAsByte<EMovementMode> SavedMovementMode = MOVE_Walking;
	uint8 SavedCustomMovementMode = 0;
	bool bHasCachedFacilityTransforms = false;
	bool bHasSavedMovementMode = false;
	bool bSnappedToFacilityActionPoint = false;
	bool bWaitingForCheckIn = false;
	bool bCheckInTerminalCommitted = false;
	bool bCheckInTimedOut = false;
	bool bCheckoutOfferActive = false;
	bool bBathStayStarted = false;
	bool bBathStayExpired = false;
	bool bCashClaimed = false;
	bool bFinished = false;
	bool bCleanupInProgress = false;
};
