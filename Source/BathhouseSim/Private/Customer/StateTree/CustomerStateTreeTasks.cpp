#include "Customer/StateTree/CustomerStateTreeTasks.h"

#include "Animation/AnimMontage.h"
#include "AIController.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerMontagePlaybackComponent.h"
#include "Customer/CustomerRoutineInterruptionComponent.h"
#include "Customer/CustomerRoutineDefinition.h"
#include "Customer/CustomerSessionComponent.h"
#include "Economy/BathhouseCashPaymentActor.h"
#include "StateTreeExecutionContext.h"
#include "Tasks/AITask_MoveTo.h"

DEFINE_LOG_CATEGORY_STATIC(LogBathhouseCustomerMontageTask, Log, All);

namespace
{
UCustomerRoutineInterruptionComponent* GetInterruption(const ABathhouseCustomerCharacter* Customer)
{
	return Customer ? Customer->GetCustomerRoutineInterruption() : nullptr;
}

UCustomerRoutineInterruptionComponent* GetInterruption(const UCustomerSessionComponent* Session)
{
	const ABathhouseCustomerCharacter* Customer = Session
		? Cast<ABathhouseCustomerCharacter>(Session->GetOwner())
		: nullptr;
	return GetInterruption(Customer);
}

bool StartMontageFacilityRecovery(
	ABathhouseCustomerCharacter* Customer,
	TObjectPtr<UAITask_MoveTo>& MoveTask,
	uint64& OperationToken)
{
	UCustomerSessionComponent* Session = Customer ? Customer->GetCustomerSession() : nullptr;
	UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Customer);
	FTransform ApproachTransform;
	AAIController* Controller = Customer ? Cast<AAIController>(Customer->GetController()) : nullptr;
	if (!Session || !Session->IsCurrentFacilityUseSuspendedForKnockdown() || !Interruption || !Controller
		|| !Session->GetCurrentFacilityTransform(true, ApproachTransform))
	{
		return false;
	}
	OperationToken = Interruption->RegisterRestartableOperation();
	MoveTask = UAITask_MoveTo::AIMoveTo(
		Controller,
		ApproachTransform.GetLocation(),
		nullptr,
		35.0f,
		EAIOptionFlag::Enable,
		EAIOptionFlag::Enable,
		true,
		false,
		false,
		EAIOptionFlag::Enable,
		EAIOptionFlag::Enable);
	if (!MoveTask)
	{
		Interruption->ClearRestartableOperation(OperationToken);
		OperationToken = 0;
		return false;
	}
	MoveTask->ReadyForActivation();
	return true;
}

EStateTreeRunStatus TickMontageFacilityRecovery(
	ABathhouseCustomerCharacter* Customer,
	TObjectPtr<UAITask_MoveTo>& MoveTask,
	uint64& OperationToken)
{
	UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Customer);
	UCustomerSessionComponent* Session = Customer ? Customer->GetCustomerSession() : nullptr;
	if (!Interruption || !Session || !Interruption->IsRestartableOperationCurrent(OperationToken))
	{
		if (MoveTask && MoveTask->IsActive())
		{
			MoveTask->ExternalCancel();
		}
		MoveTask = nullptr;
		if (Interruption && Interruption->IsRestartableOperationCurrent(OperationToken))
		{
			Interruption->ClearRestartableOperation(OperationToken);
		}
		OperationToken = 0;
		return EStateTreeRunStatus::Failed;
	}
	if (!MoveTask)
	{
		Interruption->ClearRestartableOperation(OperationToken);
		OperationToken = 0;
		return EStateTreeRunStatus::Failed;
	}
	if (MoveTask->IsActive())
	{
		return EStateTreeRunStatus::Running;
	}
	const bool bMoveSucceeded = MoveTask->WasMoveSuccessful();
	MoveTask = nullptr;
	Interruption->ClearRestartableOperation(OperationToken);
	OperationToken = 0;
	return bMoveSucceeded && Session->ResumeCurrentFacilityUseAfterKnockdown()
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

void CancelMontageFacilityRecovery(
	ABathhouseCustomerCharacter* Customer,
	TObjectPtr<UAITask_MoveTo>& MoveTask,
	uint64& OperationToken)
{
	if (MoveTask && MoveTask->IsActive())
	{
		MoveTask->ExternalCancel();
	}
	MoveTask = nullptr;
	if (UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Customer))
	{
		Interruption->ClearRestartableOperation(OperationToken);
	}
	OperationToken = 0;
}
}

UAnimMontage* BathhouseCustomerMontageTasks::SelectMontageCandidate(
	const TArray<TObjectPtr<UAnimMontage>>& Candidates,
	FRandomStream* RandomStream)
{
	TArray<UAnimMontage*, TInlineAllocator<8>> ValidCandidates;
	for (UAnimMontage* Candidate : Candidates)
	{
		if (IsValid(Candidate))
		{
			ValidCandidates.Add(Candidate);
		}
	}
	if (ValidCandidates.IsEmpty())
	{
		UE_LOG(LogBathhouseCustomerMontageTask, Error, TEXT("Customer montage task has no valid montage candidates."));
		return nullptr;
	}
	if (ValidCandidates.Num() == 1)
	{
		return ValidCandidates[0];
	}
	const int32 SelectedIndex = RandomStream
		? RandomStream->RandRange(0, ValidCandidates.Num() - 1)
		: FMath::RandHelper(ValidCandidates.Num());
	return ValidCandidates[SelectedIndex];
}

FCustomerQueueTask::FCustomerQueueTask()
{
	bShouldStateChangeOnReselect = false;
#if WITH_EDITORONLY_DATA
	bConsideredForCompletion = false;
	bCanEditConsideredForCompletion = false;
#endif
}

EStateTreeRunStatus FCustomerQueueTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	return Data.Session && Data.Session->JoinQueue(Data.Lane)
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

void FCustomerQueueTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.Session)
	{
		Data.Session->LeaveQueue();
	}
}

EStateTreeRunStatus FCustomerQueueTargetTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	FTransform Transform;
	if (!Data.Session || !Data.Session->GetQueueTargetTransform(Transform))
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.Destination = Transform.GetLocation();
	Data.Facing = Transform.Rotator();
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FCustomerWaitForKeyTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session || !Data.Session->IsQueueFront())
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.Session->BeginWaitingForCheckIn();
	return Data.Session->IsWaitingForCheckIn() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCustomerWaitForKeyTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	const FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.Session->HasAssignedKey())
	{
		return EStateTreeRunStatus::Succeeded;
	}
	if (Data.Session->DidCheckInTimeOut())
	{
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

void FCustomerWaitForKeyTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.Session)
	{
		Data.Session->CancelCheckInWait();
	}
}

FCustomerFacilityTask::FCustomerFacilityTask()
{
	bShouldStateChangeOnReselect = false;
#if WITH_EDITORONLY_DATA
	bConsideredForCompletion = false;
	bCanEditConsideredForCompletion = false;
#endif
}

EStateTreeRunStatus FCustomerFacilityTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.bReservationAcquired = Data.Session->TryReserveFacility(Data.FacilityType, Data.bExcludeLastBath);
	const UCustomerRoutineDefinition* Definition = Data.Session->GetRoutineDefinition();
	Data.RetryRemaining = Definition ? Definition->FacilityRetryIntervalSeconds : 0.5f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FCustomerFacilityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.bReservationAcquired)
	{
		return EStateTreeRunStatus::Running;
	}

	Data.RetryRemaining -= DeltaTime;
	if (Data.RetryRemaining <= 0.0f)
	{
		Data.bReservationAcquired = Data.Session->TryReserveFacility(Data.FacilityType, Data.bExcludeLastBath);
		const UCustomerRoutineDefinition* Definition = Data.Session->GetRoutineDefinition();
		Data.RetryRemaining = Definition ? Definition->FacilityRetryIntervalSeconds : 0.5f;
	}
	return EStateTreeRunStatus::Running;
}

void FCustomerFacilityTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.Session)
	{
		const EBathhouseCustomerActivity ActiveActivity = Data.Session->GetCurrentActivity();
		if (ActiveActivity != EBathhouseCustomerActivity::None)
		{
			Data.Session->AbortActivity(ActiveActivity, false);
		}
		Data.Session->SnapCurrentFacility(ECustomerFacilitySnapTarget::ApproachPoint);
		Data.Session->StopWaitingForFacility();
		Data.Session->ReleaseCurrentFacility();
	}
	Data.bReservationAcquired = false;
}

EStateTreeRunStatus FCustomerFacilityTargetTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	FTransform Transform;
	if (!Data.Session || !Data.Session->GetCurrentFacilityTransform(Data.bUseApproachPoint, Transform))
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.Destination = Transform.GetLocation();
	Data.Facing = Transform.Rotator();
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FCustomerFacilitySnapTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	return Data.Session && Data.Session->SnapCurrentFacility(Data.Target)
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCustomerBeginActivityTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.ResolvedDuration = Data.Session ? Data.Session->BeginActivity(Data.Activity) : -1.0f;
	return Data.ResolvedDuration >= 0.0f
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

FPlayCustomerMontageOnceTask::FPlayCustomerMontageOnceTask()
{
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FPlayCustomerMontageOnceTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.SelectedMontage = nullptr;
	Data.PlaybackToken = 0;
	Data.RecoveryOperationToken = 0;
	Data.bRecoveringFacilityUse = false;
	Data.RecoveryMoveTask = nullptr;
	Data.InterruptionSerial = GetInterruption(Data.Customer)
		? GetInterruption(Data.Customer)->GetInterruptionSerial()
		: 0;
	Data.SelectedMontage = BathhouseCustomerMontageTasks::SelectMontageCandidate(Data.MontageCandidates);
	UCustomerMontagePlaybackComponent* Playback = Data.Customer
		? Data.Customer->GetCustomerMontagePlayback()
		: nullptr;
	if (!Data.SelectedMontage || !Playback || !Playback->PlayMontage(
		Data.SelectedMontage,
		Data.PlayRate,
		Data.BlendInTime,
		Data.StartSection,
		NAME_None,
		Data.PlaybackToken))
	{
		UE_LOG(LogBathhouseCustomerMontageTask, Error, TEXT("Play Customer Montage Once failed to start a montage."));
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FPlayCustomerMontageOnceTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UCustomerMontagePlaybackComponent* Playback = Data.Customer
		? Data.Customer->GetCustomerMontagePlayback()
		: nullptr;
	if (!Playback)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (const UCustomerSessionComponent* Session = Data.Customer ? Data.Customer->GetCustomerSession() : nullptr;
		Session && Session->GetCurrentActivity() == EBathhouseCustomerActivity::BathDwell && Session->IsBathStayExpired())
	{
		CancelMontageFacilityRecovery(Data.Customer, Data.RecoveryMoveTask, Data.RecoveryOperationToken);
		Data.bRecoveringFacilityUse = false;
		Playback->StopPlayback(Data.PlaybackToken, Data.BlendOutTime);
		return EStateTreeRunStatus::Succeeded;
	}
	bool bRestartPlayback = false;
	if (UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Data.Customer);
		Interruption && Data.InterruptionSerial != Interruption->GetInterruptionSerial())
	{
		Data.InterruptionSerial = Interruption->GetInterruptionSerial();
		if (const UCustomerSessionComponent* Session = Data.Customer ? Data.Customer->GetCustomerSession() : nullptr;
			Session && Session->IsCurrentFacilityUseSuspendedForKnockdown())
		{
			Data.bRecoveringFacilityUse = StartMontageFacilityRecovery(
				Data.Customer,
				Data.RecoveryMoveTask,
				Data.RecoveryOperationToken);
			return Data.bRecoveringFacilityUse ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
		}
		bRestartPlayback = true;
	}
	if (Data.bRecoveringFacilityUse)
	{
		const EStateTreeRunStatus RecoveryStatus = TickMontageFacilityRecovery(
			Data.Customer,
			Data.RecoveryMoveTask,
			Data.RecoveryOperationToken);
		if (RecoveryStatus != EStateTreeRunStatus::Succeeded)
		{
			return RecoveryStatus;
		}
		Data.bRecoveringFacilityUse = false;
		bRestartPlayback = true;
	}
	if (bRestartPlayback)
	{
		Data.SelectedMontage = BathhouseCustomerMontageTasks::SelectMontageCandidate(Data.MontageCandidates);
		Data.PlaybackToken = 0;
		if (!Data.SelectedMontage || !Playback->PlayMontage(
			Data.SelectedMontage,
			Data.PlayRate,
			Data.BlendInTime,
			Data.StartSection,
			NAME_None,
			Data.PlaybackToken))
		{
			return EStateTreeRunStatus::Failed;
		}
		return EStateTreeRunStatus::Running;
	}
	return ResolvePlaybackStatus(Playback->GetPlaybackResult(Data.PlaybackToken));
}

EStateTreeRunStatus FPlayCustomerMontageOnceTask::ResolvePlaybackStatus(
	const ECustomerMontagePlaybackResult PlaybackResult)
{
	switch (PlaybackResult)
	{
	case ECustomerMontagePlaybackResult::Playing:
		return EStateTreeRunStatus::Running;
	case ECustomerMontagePlaybackResult::Succeeded:
		return EStateTreeRunStatus::Succeeded;
	case ECustomerMontagePlaybackResult::Interrupted:
	case ECustomerMontagePlaybackResult::Invalid:
	default:
		return EStateTreeRunStatus::Failed;
	}
}

void FPlayCustomerMontageOnceTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	CancelMontageFacilityRecovery(Data.Customer, Data.RecoveryMoveTask, Data.RecoveryOperationToken);
	Data.bRecoveringFacilityUse = false;
	if (UCustomerMontagePlaybackComponent* Playback = Data.Customer
		? Data.Customer->GetCustomerMontagePlayback()
		: nullptr)
	{
		Playback->StopPlayback(Data.PlaybackToken, Data.BlendOutTime);
	}
}

FPlaySelectedMontageLoopForDurationTask::FPlaySelectedMontageLoopForDurationTask()
{
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FPlaySelectedMontageLoopForDurationTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.SelectedMontage = nullptr;
	Data.PlaybackToken = 0;
	Data.RecoveryOperationToken = 0;
	Data.bRecoveringFacilityUse = false;
	Data.RecoveryMoveTask = nullptr;
	Data.RemainingDuration = Data.Duration;
	Data.InterruptionSerial = GetInterruption(Data.Customer)
		? GetInterruption(Data.Customer)->GetInterruptionSerial()
		: 0;
	Data.SelectedMontage = BathhouseCustomerMontageTasks::SelectMontageCandidate(Data.MontageCandidates);
	UCustomerMontagePlaybackComponent* Playback = Data.Customer
		? Data.Customer->GetCustomerMontagePlayback()
		: nullptr;
	if (Data.Duration < 0.1f || Data.LoopSection.IsNone() || !Data.SelectedMontage
		|| !Data.SelectedMontage->IsValidSectionName(Data.LoopSection)
		|| !Playback || !Playback->PlayMontage(
			Data.SelectedMontage,
			Data.PlayRate,
			Data.BlendInTime,
			Data.LoopSection,
			Data.LoopSection,
			Data.PlaybackToken))
	{
		UE_LOG(LogBathhouseCustomerMontageTask, Error, TEXT("Play Selected Montage Loop For Duration has invalid parameters or failed to start."));
		return EStateTreeRunStatus::Failed;
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FPlaySelectedMontageLoopForDurationTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UCustomerMontagePlaybackComponent* Playback = Data.Customer
		? Data.Customer->GetCustomerMontagePlayback()
		: nullptr;
	if (!Playback)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (const UCustomerSessionComponent* Session = Data.Customer ? Data.Customer->GetCustomerSession() : nullptr;
		Session && Session->GetCurrentActivity() == EBathhouseCustomerActivity::BathDwell && Session->IsBathStayExpired())
	{
		CancelMontageFacilityRecovery(Data.Customer, Data.RecoveryMoveTask, Data.RecoveryOperationToken);
		Data.bRecoveringFacilityUse = false;
		Playback->StopPlayback(Data.PlaybackToken, Data.BlendOutTime);
		return EStateTreeRunStatus::Succeeded;
	}
	bool bRestartPlayback = false;
	if (UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Data.Customer);
		Interruption && Data.InterruptionSerial != Interruption->GetInterruptionSerial())
	{
		Data.InterruptionSerial = Interruption->GetInterruptionSerial();
		if (const UCustomerSessionComponent* Session = Data.Customer ? Data.Customer->GetCustomerSession() : nullptr;
			Session && Session->IsCurrentFacilityUseSuspendedForKnockdown())
		{
			Data.bRecoveringFacilityUse = StartMontageFacilityRecovery(
				Data.Customer,
				Data.RecoveryMoveTask,
				Data.RecoveryOperationToken);
			return Data.bRecoveringFacilityUse ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
		}
		bRestartPlayback = true;
	}
	if (Data.bRecoveringFacilityUse)
	{
		const EStateTreeRunStatus RecoveryStatus = TickMontageFacilityRecovery(
			Data.Customer,
			Data.RecoveryMoveTask,
			Data.RecoveryOperationToken);
		if (RecoveryStatus != EStateTreeRunStatus::Succeeded)
		{
			return RecoveryStatus;
		}
		Data.bRecoveringFacilityUse = false;
		bRestartPlayback = true;
	}
	if (bRestartPlayback)
	{
		Data.RemainingDuration = Data.Duration;
		if (const UCustomerSessionComponent* Session = Data.Customer ? Data.Customer->GetCustomerSession() : nullptr;
			Session && Session->GetCurrentActivity() == EBathhouseCustomerActivity::BathDwell)
		{
			Data.RemainingDuration = FMath::Min(Data.RemainingDuration, Session->GetRemainingBathStaySeconds());
		}
		Data.SelectedMontage = BathhouseCustomerMontageTasks::SelectMontageCandidate(Data.MontageCandidates);
		Data.PlaybackToken = 0;
		if (!Data.SelectedMontage || !Data.SelectedMontage->IsValidSectionName(Data.LoopSection)
			|| !Playback->PlayMontage(
				Data.SelectedMontage,
				Data.PlayRate,
				Data.BlendInTime,
				Data.LoopSection,
				Data.LoopSection,
				Data.PlaybackToken))
		{
			return EStateTreeRunStatus::Failed;
		}
		return EStateTreeRunStatus::Running;
	}

	Data.RemainingDuration -= DeltaTime;
	const EStateTreeRunStatus PlaybackStatus = ResolvePlaybackStatus(
		Playback->GetPlaybackResult(Data.PlaybackToken),
		Data.RemainingDuration);
	if (PlaybackStatus == EStateTreeRunStatus::Failed)
	{
		UE_LOG(LogBathhouseCustomerMontageTask, Error, TEXT("Selected customer loop montage ended before its requested duration."));
		return EStateTreeRunStatus::Failed;
	}
	if (PlaybackStatus == EStateTreeRunStatus::Succeeded)
	{
		if (!Playback->StopPlayback(Data.PlaybackToken, Data.BlendOutTime))
		{
			return EStateTreeRunStatus::Failed;
		}
		return EStateTreeRunStatus::Succeeded;
	}
	return PlaybackStatus;
}

EStateTreeRunStatus FPlaySelectedMontageLoopForDurationTask::ResolvePlaybackStatus(
	const ECustomerMontagePlaybackResult PlaybackResult,
	const float RemainingDuration)
{
	if (PlaybackResult != ECustomerMontagePlaybackResult::Playing)
	{
		return EStateTreeRunStatus::Failed;
	}
	return RemainingDuration <= 0.0f
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

void FPlaySelectedMontageLoopForDurationTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	CancelMontageFacilityRecovery(Data.Customer, Data.RecoveryMoveTask, Data.RecoveryOperationToken);
	Data.bRecoveringFacilityUse = false;
	if (UCustomerMontagePlaybackComponent* Playback = Data.Customer
		? Data.Customer->GetCustomerMontagePlayback()
		: nullptr)
	{
		Playback->StopPlayback(Data.PlaybackToken, Data.BlendOutTime);
	}
}

EStateTreeRunStatus FCustomerFinishActivityTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session || Data.Session->GetCurrentActivity() != Data.Activity)
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.Session->FinishActivity(Data.Activity);
	return Data.Session->GetCurrentActivity() == EBathhouseCustomerActivity::None
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCustomerActivityTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.bCompleted = false;
	Data.bRecoveringFacilityUse = false;
	Data.RecoveryOperationToken = 0;
	Data.RecoveryMoveTask = nullptr;
	Data.RemainingTime = Data.Session ? Data.Session->BeginActivity(Data.Activity) : -1.0f;
	Data.InterruptionSerial = GetInterruption(Data.Session)
		? GetInterruption(Data.Session)->GetInterruptionSerial()
		: 0;
	return Data.RemainingTime >= 0.0f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCustomerActivityTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.Activity == EBathhouseCustomerActivity::BathDwell && Data.Session->IsBathStayExpired())
	{
		CancelFacilityRecoveryMove(Data);
		Data.bCompleted = true;
		return EStateTreeRunStatus::Succeeded;
	}
	if (UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Data.Session);
		Interruption && Data.InterruptionSerial != Interruption->GetInterruptionSerial())
	{
		Data.InterruptionSerial = Interruption->GetInterruptionSerial();
		if (Data.Session->IsCurrentFacilityUseSuspendedForKnockdown())
		{
			Data.RecoveryOperationToken = Interruption->RegisterRestartableOperation();
			Data.bRecoveringFacilityUse = StartFacilityRecoveryMove(Data);
			if (!Data.bRecoveringFacilityUse)
			{
				Interruption->ClearRestartableOperation(Data.RecoveryOperationToken);
				Data.RecoveryOperationToken = 0;
				return EStateTreeRunStatus::Failed;
			}
			return EStateTreeRunStatus::Running;
		}
		Data.RemainingTime = Data.Session->RestartCurrentActivity(Data.Activity);
		return Data.RemainingTime >= 0.0f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
	}
	if (Data.bRecoveringFacilityUse)
	{
		UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Data.Session);
		if (!Interruption || !Interruption->IsRestartableOperationCurrent(Data.RecoveryOperationToken))
		{
			CancelFacilityRecoveryMove(Data);
			Data.RecoveryOperationToken = 0;
			Data.bRecoveringFacilityUse = false;
			return EStateTreeRunStatus::Failed;
		}
		if (!Data.RecoveryMoveTask)
		{
			Interruption->ClearRestartableOperation(Data.RecoveryOperationToken);
			Data.RecoveryOperationToken = 0;
			Data.bRecoveringFacilityUse = false;
			return EStateTreeRunStatus::Failed;
		}
		if (Data.RecoveryMoveTask->IsActive())
		{
			return EStateTreeRunStatus::Running;
		}
		const bool bMoveSucceeded = Data.RecoveryMoveTask->WasMoveSuccessful();
		CancelFacilityRecoveryMove(Data);
		Interruption->ClearRestartableOperation(Data.RecoveryOperationToken);
		Data.RecoveryOperationToken = 0;
		Data.bRecoveringFacilityUse = false;
		if (!bMoveSucceeded || !Data.Session->ResumeCurrentFacilityUseAfterKnockdown())
		{
			return EStateTreeRunStatus::Failed;
		}
		Data.RemainingTime = Data.Session->RestartCurrentActivity(Data.Activity);
		return Data.RemainingTime >= 0.0f ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
	}
	Data.RemainingTime -= DeltaTime;
	if (Data.RemainingTime <= 0.0f)
	{
		Data.Session->FinishActivity(Data.Activity);
		Data.Session->ReleaseCurrentFacility();
		Data.bCompleted = true;
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

void FCustomerActivityTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	CancelFacilityRecoveryMove(Data);
	if (UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Data.Session))
	{
		Interruption->ClearRestartableOperation(Data.RecoveryOperationToken);
	}
	Data.RecoveryOperationToken = 0;
	Data.bRecoveringFacilityUse = false;
	if (Data.Session && !Data.bCompleted)
	{
		Data.Session->AbortActivity(Data.Activity);
	}
}

bool FCustomerActivityTask::StartFacilityRecoveryMove(FInstanceDataType& Data)
{
	FTransform ApproachTransform;
	ABathhouseCustomerCharacter* Customer = Data.Session
		? Cast<ABathhouseCustomerCharacter>(Data.Session->GetOwner())
		: nullptr;
	AAIController* Controller = Customer ? Cast<AAIController>(Customer->GetController()) : nullptr;
	if (!Controller || !Data.Session->GetCurrentFacilityTransform(true, ApproachTransform))
	{
		return false;
	}
	Data.RecoveryMoveTask = UAITask_MoveTo::AIMoveTo(
		Controller,
		ApproachTransform.GetLocation(),
		nullptr,
		35.0f,
		EAIOptionFlag::Enable,
		EAIOptionFlag::Enable,
		true,
		false,
		false,
		EAIOptionFlag::Enable,
		EAIOptionFlag::Enable);
	if (!Data.RecoveryMoveTask)
	{
		return false;
	}
	Data.RecoveryMoveTask->ReadyForActivation();
	return true;
}

void FCustomerActivityTask::CancelFacilityRecoveryMove(FInstanceDataType& Data)
{
	if (Data.RecoveryMoveTask && Data.RecoveryMoveTask->IsActive())
	{
		Data.RecoveryMoveTask->ExternalCancel();
	}
	Data.RecoveryMoveTask = nullptr;
}

EStateTreeRunStatus FCustomerStartBathStayTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	return Data.Session && Data.Session->StartBathStay()
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCustomerCheckoutOfferTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.bCompleted = false;
	Data.RetryRemaining = 0.0f;
	Data.InterruptionSerial = GetInterruption(Data.Session)
		? GetInterruption(Data.Session)->GetInterruptionSerial()
		: 0;
	if (!Data.Session || !Data.CashClass || !Data.Session->BeginCheckoutOffer())
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.Session->TryPlaceCheckoutKey())
	{
		Data.Session->TryCreateCashOffer(Data.CashClass);
	}
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FCustomerCheckoutOfferTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Data.Session);
		Interruption && Data.InterruptionSerial != Interruption->GetInterruptionSerial())
	{
		Data.InterruptionSerial = Interruption->GetInterruptionSerial();
		Data.RetryRemaining = 0.0f;
	}
	if (Data.Session->IsCashClaimed())
	{
		Data.bCompleted = true;
		return EStateTreeRunStatus::Succeeded;
	}

	Data.RetryRemaining -= DeltaTime;
	if (Data.RetryRemaining <= 0.0f)
	{
		if (Data.Session->TryPlaceCheckoutKey())
		{
			Data.Session->TryCreateCashOffer(Data.CashClass);
		}
		const UCustomerRoutineDefinition* Definition = Data.Session->GetRoutineDefinition();
		Data.RetryRemaining = Definition ? Definition->FacilityRetryIntervalSeconds : 0.5f;
	}
	return EStateTreeRunStatus::Running;
}

void FCustomerCheckoutOfferTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return;
	}
	Data.Session->EndCheckoutOffer();
	if (!Data.bCompleted && !Data.Session->IsCashClaimed())
	{
		Data.Session->CancelCashOffer();
		Data.Session->TechnicalAbort(TEXT("Checkout offer state exited before cash claim."));
	}
}

FCustomerRestartableMoveToTask::FCustomerRestartableMoveToTask()
{
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FCustomerRestartableMoveToTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.bMoveSucceeded = false;
	Data.MoveTask = nullptr;
	UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Data.Customer);
	Data.InterruptionSerial = Interruption ? Interruption->GetInterruptionSerial() : 0;
	Data.OperationToken = Interruption ? Interruption->RegisterRestartableOperation() : 0;
	if (StartMove(Data))
	{
		return EStateTreeRunStatus::Running;
	}
	if (Interruption)
	{
		Interruption->ClearRestartableOperation(Data.OperationToken);
	}
	Data.OperationToken = 0;
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCustomerRestartableMoveToTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Data.Customer);
	if (Interruption && Interruption->IsSoftInterrupted())
	{
		return EStateTreeRunStatus::Running;
	}
	if (Interruption && Data.InterruptionSerial != Interruption->GetInterruptionSerial())
	{
		CancelMove(Data);
		Data.InterruptionSerial = Interruption->GetInterruptionSerial();
		Data.OperationToken = Interruption->RegisterRestartableOperation();
		if (StartMove(Data))
		{
			return EStateTreeRunStatus::Running;
		}
		Interruption->ClearRestartableOperation(Data.OperationToken);
		Data.OperationToken = 0;
		return EStateTreeRunStatus::Failed;
	}
	if (Interruption && !Interruption->IsRestartableOperationCurrent(Data.OperationToken))
	{
		return InvalidateSupersededOperation(Data);
	}
	if (!Data.MoveTask)
	{
		if (Interruption)
		{
			Interruption->ClearRestartableOperation(Data.OperationToken);
		}
		Data.OperationToken = 0;
		return EStateTreeRunStatus::Failed;
	}
	if (Data.MoveTask->IsActive())
	{
		return EStateTreeRunStatus::Running;
	}
	Data.bMoveSucceeded = Data.MoveTask->WasMoveSuccessful();
	Data.MoveTask = nullptr;
	if (Interruption)
	{
		Interruption->ClearRestartableOperation(Data.OperationToken);
	}
	Data.OperationToken = 0;
	return Data.bMoveSucceeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

void FCustomerRestartableMoveToTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	CancelMove(Data);
	if (UCustomerRoutineInterruptionComponent* Interruption = GetInterruption(Data.Customer))
	{
		Interruption->ClearRestartableOperation(Data.OperationToken);
	}
	Data.OperationToken = 0;
}

bool FCustomerRestartableMoveToTask::StartMove(FInstanceDataType& Data)
{
	AAIController* Controller = Data.Customer ? Cast<AAIController>(Data.Customer->GetController()) : nullptr;
	if (!Controller || Data.Destination.ContainsNaN())
	{
		return false;
	}
	Data.MoveTask = UAITask_MoveTo::AIMoveTo(
		Controller,
		Data.Destination,
		nullptr,
		FMath::Max(0.0f, Data.AcceptanceRadius),
		EAIOptionFlag::Enable,
		Data.bAllowPartialPath ? EAIOptionFlag::Enable : EAIOptionFlag::Disable,
		true,
		false,
		false,
		Data.bProjectGoalOnNavigation ? EAIOptionFlag::Enable : EAIOptionFlag::Disable,
		EAIOptionFlag::Enable);
	if (!Data.MoveTask)
	{
		return false;
	}
	Data.MoveTask->ReadyForActivation();
	return true;
}

void FCustomerRestartableMoveToTask::CancelMove(FInstanceDataType& Data)
{
	if (Data.MoveTask && Data.MoveTask->IsActive())
	{
		Data.MoveTask->ExternalCancel();
	}
	Data.MoveTask = nullptr;
}

EStateTreeRunStatus FCustomerRestartableMoveToTask::InvalidateSupersededOperation(FInstanceDataType& Data)
{
	CancelMove(Data);
	Data.OperationToken = 0;
	Data.bMoveSucceeded = false;
	return EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCustomerNavigationResultTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.bMoveSucceeded)
	{
		Data.Session->ResetNavigationFailures();
		Data.bRetriesExhausted = false;
	}
	else
	{
		Data.bRetriesExhausted = Data.Session->RegisterNavigationFailure();
	}
	return EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FCustomerFinishTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	EBathhouseCustomerDepartureReason FinalReason = Data.Reason;
	if (Data.Session->IsTechnicalAbort())
	{
		FinalReason = EBathhouseCustomerDepartureReason::TechnicalAbort;
	}
	else if (Data.Session->DidCheckInTimeOut())
	{
		FinalReason = EBathhouseCustomerDepartureReason::CheckInTimedOut;
	}
	Data.Session->FinishSession(FinalReason);
	return EStateTreeRunStatus::Succeeded;
}
