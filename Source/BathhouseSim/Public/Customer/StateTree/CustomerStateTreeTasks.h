#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "Customer/BathhouseCustomerTypes.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "CustomerStateTreeTasks.generated.h"

class ABathhouseCashPaymentActor;
class ABathhouseCustomerCharacter;
class UAnimMontage;
class UCustomerSessionComponent;
class UAITask_MoveTo;

namespace BathhouseCustomerMontageTasks
{
	BATHHOUSESIM_API UAnimMontage* SelectMontageCandidate(
		const TArray<TObjectPtr<UAnimMontage>>& Candidates,
		FRandomStream* RandomStream = nullptr);
}

USTRUCT()
struct FCustomerQueueTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EBathhouseCounterLane Lane = EBathhouseCounterLane::CheckIn;
};

USTRUCT(meta = (DisplayName = "Hold Customer Queue", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerQueueTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerQueueTaskInstanceData;
	FCustomerQueueTask();
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerQueueTargetTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Output)
	FVector Destination = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Output)
	FRotator Facing = FRotator::ZeroRotator;
};

USTRUCT(meta = (DisplayName = "Get Customer Queue Target (Deprecated)", Category = "Bathhouse|Customer", Deprecated, DeprecationMessage = "Replace with Move To Current Queue Assignment."))
struct BATHHOUSESIM_API FCustomerQueueTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerQueueTargetTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerWaitForKeyTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;
};

USTRUCT(meta = (DisplayName = "Wait For Check-In Key", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerWaitForKeyTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerWaitForKeyTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerFacilityTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EBathhouseFacilityType FacilityType = EBathhouseFacilityType::Shower;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bExcludeLastBath = false;

	float RetryRemaining = 0.0f;
	bool bReservationAcquired = false;
};

USTRUCT(meta = (DisplayName = "Hold Customer Facility", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerFacilityTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerFacilityTaskInstanceData;
	FCustomerFacilityTask();
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerFacilityTargetTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bUseApproachPoint = true;

	UPROPERTY(EditAnywhere, Category = Output)
	FVector Destination = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Output)
	FRotator Facing = FRotator::ZeroRotator;
};

USTRUCT(meta = (DisplayName = "Get Customer Facility Target", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerFacilityTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerFacilityTargetTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerFacilitySnapTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	ECustomerFacilitySnapTarget Target = ECustomerFacilitySnapTarget::ActionPoint;
};

USTRUCT(meta = (DisplayName = "Snap Customer Facility Point", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerFacilitySnapTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerFacilitySnapTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerBeginActivityTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EBathhouseCustomerActivity Activity = EBathhouseCustomerActivity::StoreShoes;

	UPROPERTY(EditAnywhere, Category = Output)
	float ResolvedDuration = 0.0f;
};

USTRUCT(meta = (DisplayName = "Begin Customer Activity", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerBeginActivityTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerBeginActivityTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FPlayCustomerMontageOnceTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ABathhouseCustomerCharacter> Customer = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<TObjectPtr<UAnimMontage>> MontageCandidates;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FName StartSection = NAME_None;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0"))
	float BlendInTime = 0.2f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0"))
	float BlendOutTime = 0.2f;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> SelectedMontage = nullptr;

	uint64 PlaybackToken = 0;
	uint64 InterruptionSerial = 0;
	uint64 RecoveryOperationToken = 0;
	bool bRecoveringFacilityUse = false;

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> RecoveryMoveTask = nullptr;
};

USTRUCT(meta = (DisplayName = "Play Customer Montage Once", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FPlayCustomerMontageOnceTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPlayCustomerMontageOnceTaskInstanceData;
	FPlayCustomerMontageOnceTask();
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	friend class FBathhouseCustomerMontageTest;
	static EStateTreeRunStatus ResolvePlaybackStatus(ECustomerMontagePlaybackResult PlaybackResult);
};

USTRUCT()
struct FPlaySelectedMontageLoopForDurationTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ABathhouseCustomerCharacter> Customer = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<TObjectPtr<UAnimMontage>> MontageCandidates;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.1"))
	float Duration = 1.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.01"))
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FName LoopSection = NAME_None;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0"))
	float BlendInTime = 0.2f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0"))
	float BlendOutTime = 0.2f;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> SelectedMontage = nullptr;

	float RemainingDuration = 0.0f;
	uint64 PlaybackToken = 0;
	uint64 InterruptionSerial = 0;
	uint64 RecoveryOperationToken = 0;
	bool bRecoveringFacilityUse = false;

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> RecoveryMoveTask = nullptr;
};

USTRUCT(meta = (DisplayName = "Play Selected Montage Loop For Duration", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FPlaySelectedMontageLoopForDurationTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPlaySelectedMontageLoopForDurationTaskInstanceData;
	FPlaySelectedMontageLoopForDurationTask();
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	friend class FBathhouseCustomerMontageTest;
	static EStateTreeRunStatus ResolvePlaybackStatus(
		ECustomerMontagePlaybackResult PlaybackResult,
		float RemainingDuration);
};

USTRUCT()
struct FCustomerFinishActivityTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EBathhouseCustomerActivity Activity = EBathhouseCustomerActivity::StoreShoes;
};

USTRUCT(meta = (DisplayName = "Finish Customer Activity", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerFinishActivityTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerFinishActivityTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerActivityTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EBathhouseCustomerActivity Activity = EBathhouseCustomerActivity::StoreShoes;

	float RemainingTime = 0.0f;
	bool bCompleted = false;
	bool bRecoveringFacilityUse = false;
	uint64 InterruptionSerial = 0;
	uint64 RecoveryOperationToken = 0;

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> RecoveryMoveTask = nullptr;
};

USTRUCT(meta = (DisplayName = "Timed Customer Activity", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerActivityTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerActivityTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	static bool StartFacilityRecoveryMove(FInstanceDataType& Data);
	static void CancelFacilityRecoveryMove(FInstanceDataType& Data);
};

USTRUCT()
struct FCustomerStartBathStayTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;
};

USTRUCT(meta = (DisplayName = "Start Customer Bath Stay", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerStartBathStayTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerStartBathStayTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerCheckoutOfferTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TSubclassOf<ABathhouseCashPaymentActor> CashClass;

	float RetryRemaining = 0.0f;
	bool bCompleted = false;
	uint64 InterruptionSerial = 0;
};

USTRUCT(meta = (DisplayName = "Offer Checkout Key And Cash", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerCheckoutOfferTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerCheckoutOfferTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerRestartableMoveToTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<ABathhouseCustomerCharacter> Customer = nullptr;

	UPROPERTY(EditAnywhere, Category = Input)
	FVector Destination = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 35.0f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bAllowPartialPath = true;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bProjectGoalOnNavigation = true;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bMoveSucceeded = false;

	UPROPERTY(Transient)
	TObjectPtr<UAITask_MoveTo> MoveTask = nullptr;

	uint64 InterruptionSerial = 0;
	uint64 OperationToken = 0;
};

USTRUCT(meta = (DisplayName = "Restartable Customer Move To", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerRestartableMoveToTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerRestartableMoveToTaskInstanceData;
	FCustomerRestartableMoveToTask();
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	friend class FBathhouseCustomerRecoveryFacilityAndOperationTest;

	static bool StartMove(FInstanceDataType& Data);
	static void CancelMove(FInstanceDataType& Data);
	static EStateTreeRunStatus InvalidateSupersededOperation(FInstanceDataType& Data);
};

USTRUCT()
struct FCustomerNavigationResultTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bMoveSucceeded = true;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bRetriesExhausted = false;
};

USTRUCT(meta = (DisplayName = "Record Customer Navigation Result", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerNavigationResultTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerNavigationResultTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT()
struct FCustomerFinishTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UCustomerSessionComponent> Session = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	EBathhouseCustomerDepartureReason Reason = EBathhouseCustomerDepartureReason::Completed;
};

USTRUCT(meta = (DisplayName = "Finish Customer Session", Category = "Bathhouse|Customer"))
struct BATHHOUSESIM_API FCustomerFinishTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FCustomerFinishTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
