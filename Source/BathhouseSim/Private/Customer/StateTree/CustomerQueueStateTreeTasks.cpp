#include "Customer/StateTree/CustomerQueueStateTreeTasks.h"

#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerQueueNavigationComponent.h"
#include "Customer/CustomerRoutineInterruptionComponent.h"
#include "Customer/CustomerSessionComponent.h"
#include "StateTreeExecutionContext.h"

FCustomerMoveToCurrentQueueAssignmentTask::FCustomerMoveToCurrentQueueAssignmentTask()
{
	bShouldStateChangeOnReselect = false;
}

EStateTreeRunStatus FCustomerMoveToCurrentQueueAssignmentTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Customer || !Data.Session || Data.Session->GetOwner() != Data.Customer
		|| Data.ExpectedLane == EBathhouseCounterLane::None
		|| Data.Session->GetQueueLane() != Data.ExpectedLane)
	{
		return EStateTreeRunStatus::Failed;
	}
	UCustomerQueueNavigationComponent* Navigation = Data.Customer->GetCustomerQueueNavigation();
	Data.ExecutionToken = Navigation ? Navigation->BeginQueueNavigation(Data.ExpectedLane) : 0;
	return Data.ExecutionToken != 0 ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FCustomerMoveToCurrentQueueAssignmentTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (!Data.Customer || !Data.Session || Data.Session->GetOwner() != Data.Customer
		|| Data.Session->GetQueueLane() != Data.ExpectedLane)
	{
		return EStateTreeRunStatus::Failed;
	}
	if (const UCustomerRoutineInterruptionComponent* Interruption = Data.Customer->GetCustomerRoutineInterruption();
		Interruption && Interruption->IsSoftInterrupted())
	{
		return EStateTreeRunStatus::Running;
	}
	const UCustomerQueueNavigationComponent* Navigation = Data.Customer->GetCustomerQueueNavigation();
	switch (Navigation ? Navigation->GetQueueNavigationStatus(Data.ExecutionToken) : ECustomerQueueNavigationStatus::Inactive)
	{
	case ECustomerQueueNavigationStatus::ServiceReady:
		return EStateTreeRunStatus::Succeeded;
	case ECustomerQueueNavigationStatus::Failed:
	case ECustomerQueueNavigationStatus::Inactive:
		return EStateTreeRunStatus::Failed;
	default:
		return EStateTreeRunStatus::Running;
	}
}

void FCustomerMoveToCurrentQueueAssignmentTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& Data = Context.GetInstanceData(*this);
	if (Data.Customer)
	{
		if (UCustomerQueueNavigationComponent* Navigation = Data.Customer->GetCustomerQueueNavigation())
		{
			Navigation->CancelQueueNavigation(Data.ExecutionToken);
		}
	}
	Data.ExecutionToken = 0;
}
