#include "Customer/StateTree/CustomerTowelStateTreeTasks.h"

#include "Customer/CustomerSessionComponent.h"
#include "StateTreeExecutionContext.h"

FCustomerAcquireTowelTask::FCustomerAcquireTowelTask()
{
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FCustomerAcquireTowelTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	Data.bProceedingWithoutTowel = false;
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.Session->TryAcquireCleanTowelFromCurrentFacility())
	{
		return EStateTreeRunStatus::Succeeded;
	}
	return Data.Session->BeginWaitingForCleanTowel()
		? EStateTreeRunStatus::Running
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCustomerAcquireTowelTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	(void)DeltaTime;
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.Session->HasTowelHandle())
	{
		return EStateTreeRunStatus::Succeeded;
	}
	if (Data.Session->IsTowelWaitExpired())
	{
		Data.bProceedingWithoutTowel = true;
		return EStateTreeRunStatus::Succeeded;
	}
	if (Data.Session->TryAcquireCleanTowelFromCurrentFacility())
	{
		return EStateTreeRunStatus::Succeeded;
	}
	return EStateTreeRunStatus::Running;
}

void FCustomerAcquireTowelTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.Session)
	{
		Data.Session->CancelWaitingForCleanTowel();
	}
}

EStateTreeRunStatus FCustomerMarkTowelUsedTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.bHadTowel = Data.Session->HasTowelHandle();
	return Data.Session->MarkTowelUsed()
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCustomerReturnTowelTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Session)
	{
		return EStateTreeRunStatus::Failed;
	}
	Data.bHadTowel = Data.Session->HasTowelHandle();
	return Data.Session->ReturnTowelToCurrentFacility()
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}
