#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "CustomerQueueNavigationComponent.generated.h"

class ABathhouseCounterActor;
class UAITask_MoveTo;
class UCustomerSessionComponent;

enum class ECustomerQueueNavigationStatus : uint8
{
	Inactive,
	Running,
	Waiting,
	ServiceReady,
	Suspended,
	Failed
};

DECLARE_DELEGATE_OneParam(FOnCustomerQueueRecoveryFinished, bool);

UCLASS(ClassGroup = (Customer), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UCustomerQueueNavigationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCustomerQueueNavigationComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	uint64 BeginQueueNavigation(EBathhouseCounterLane ExpectedLane);
	void CancelQueueNavigation(uint64 ExecutionToken);
	ECustomerQueueNavigationStatus GetQueueNavigationStatus(uint64 ExecutionToken) const;

	void SuspendForKnockdown();
	void ResumeQueueNavigationAfterOverflowInterruption();
	bool BeginQueuePoseRecovery(FOnCustomerQueueRecoveryFinished Completion);
	void CancelQueuePoseRecovery();

private:
	friend class UCustomerSessionComponent;
	friend class FBathhouseQueueNavigationFacingTest;
	void CancelForIntentionalQueueLeave();

	enum class EExecutionMode : uint8
	{
		None,
		MovingVisible,
		FacingVisible,
		MovingOverflow,
		OverflowPause
	};

	bool InitializeExecution(EBathhouseCounterLane ExpectedLane, bool bRecoveryOnly);
	bool RefreshAssignment(bool bForceRestart);
	bool StartMove(const FVector& Destination, float AcceptanceRadius, EExecutionMode MoveMode);
	void StartFacing();
	void HandleMoveFinished(bool bSucceeded, uint64 CompletedMoveToken);
	void HandleNavigationFailure();
	void CompleteVisibleTarget();
	void CompleteRecovery(bool bSucceeded);
	void HandleQueueChanged(EBathhouseCounterLane ChangedLane);
	void CancelActiveMove();
	void CleanupExecution();
	void SnapshotMovementFlags();
	void ApplyMovementFlagsForMove();
	void ApplyMovementFlagsForFacing();
	void RestoreMovementFlags();
	uint64 AllocateNonZeroToken(uint64& Counter);
	bool IsMaterialAssignmentChange(const FBathhouseQueueAssignment& NewAssignment) const;

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> ActiveMoveTask = nullptr;

	TWeakObjectPtr<ABathhouseCounterActor> BoundCounter;
	FDelegateHandle QueueChangedHandle;
	FBathhouseQueueAssignment CurrentAssignment;
	FOnCustomerQueueRecoveryFinished RecoveryCompletion;
	uint64 NextExecutionToken = 0;
	uint64 ActiveExecutionToken = 0;
	uint64 NextMoveToken = 0;
	uint64 ActiveMoveToken = 0;
	EBathhouseCounterLane ActiveLane = EBathhouseCounterLane::None;
	ECustomerQueueNavigationStatus Status = ECustomerQueueNavigationStatus::Inactive;
	EExecutionMode Mode = EExecutionMode::None;
	float OverflowPauseRemaining = 0.0f;
	bool bSuspended = false;
	bool bRecoveryGate = false;
	bool bRecoveryOnlyExecution = false;
	bool bMovementFlagsSnapshotted = false;
	bool bSavedOrientRotationToMovement = false;
	bool bSavedUseControllerDesiredRotation = false;
	bool bSavedUseControllerRotationYaw = false;
};
