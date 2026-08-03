#include "Customer/StateTree/CustomerStateTreeTasks.h"

#include "Animation/AnimMontage.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerMontagePlaybackComponent.h"
#include "Customer/CustomerRoutineDefinition.h"
#include "Customer/CustomerSessionComponent.h"
#include "Economy/BathhouseCashPaymentActor.h"
#include "StateTreeExecutionContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogBathhouseCustomerMontageTask, Log, All);

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
	const UCustomerMontagePlaybackComponent* Playback = Data.Customer
		? Data.Customer->GetCustomerMontagePlayback()
		: nullptr;
	if (!Playback)
	{
		return EStateTreeRunStatus::Failed;
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
	Data.RemainingDuration = Data.Duration;
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
	Data.RemainingTime = Data.Session ? Data.Session->BeginActivity(Data.Activity) : -1.0f;
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
		Data.bCompleted = true;
		return EStateTreeRunStatus::Succeeded;
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
	if (Data.Session && !Data.bCompleted)
	{
		Data.Session->AbortActivity(Data.Activity);
	}
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
