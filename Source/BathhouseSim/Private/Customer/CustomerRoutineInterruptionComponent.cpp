#include "Customer/CustomerRoutineInterruptionComponent.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerQueueNavigationComponent.h"
#include "Customer/CustomerSessionComponent.h"
#include "Facility/BathhouseCounterActor.h"

UCustomerRoutineInterruptionComponent::UCustomerRoutineInterruptionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCustomerRoutineInterruptionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner()))
	{
		if (UCustomerQueueNavigationComponent* QueueNavigation = Customer->GetCustomerQueueNavigation())
		{
			QueueNavigation->CancelQueuePoseRecovery();
		}
	}
	ActiveOperationToken = 0;
	bSoftInterrupted = false;
	bQueueRecoveryPending = false;
	Super::EndPlay(EndPlayReason);
}

bool UCustomerRoutineInterruptionComponent::BeginSoftInterruption()
{
	if (bSoftInterrupted)
	{
		return false;
	}
	bSoftInterrupted = true;
	InterruptionSerial = AllocateNonZeroToken(InterruptionSerial);
	ActiveOperationToken = 0;

	if (const ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner()))
	{
		if (UCustomerQueueNavigationComponent* QueueNavigation = Customer->GetCustomerQueueNavigation())
		{
			QueueNavigation->SuspendForKnockdown();
		}
		if (UCustomerSessionComponent* Session = Customer->GetCustomerSession())
		{
			Session->PauseRoutineTimers();
			Session->SuspendCurrentFacilityUseForKnockdown();
		}
	}
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (const AAIController* Controller = Cast<AAIController>(Pawn->GetController()))
		{
			if (UBrainComponent* Brain = Controller->GetBrainComponent())
			{
				Brain->PauseLogic(TEXT("Customer knockdown soft interruption"));
			}
		}
	}
	return true;
}

bool UCustomerRoutineInterruptionComponent::EndSoftInterruption()
{
	if (!bSoftInterrupted || bQueueRecoveryPending)
	{
		return false;
	}
	const ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner());
	UCustomerSessionComponent* Session = Customer ? Customer->GetCustomerSession() : nullptr;
	UCustomerQueueNavigationComponent* QueueNavigation = Customer ? Customer->GetCustomerQueueNavigation() : nullptr;
	if (!Session || Session->GetQueueLane() == EBathhouseCounterLane::None)
	{
		CompleteSoftInterruptionResume();
		return true;
	}

	ABathhouseCounterActor* Counter = Session->GetCounter();
	FBathhouseQueueAssignment Assignment;
	if (!Counter || !Counter->ResolveQueueAssignment(Session->GetQueueLane(), GetOwner(), Assignment))
	{
		Session->TechnicalAbort(TEXT("Queue assignment was invalid when customer knockdown recovery ended."));
		CompleteSoftInterruptionResume();
		return true;
	}
	if (Assignment.Type == EBathhouseQueueAssignmentType::OverflowWander)
	{
		if (QueueNavigation)
		{
			QueueNavigation->ResumeQueueNavigationAfterOverflowInterruption();
		}
		CompleteSoftInterruptionResume();
		return true;
	}
	if (!Assignment.IsVisibleAssignment() || !QueueNavigation)
	{
		Session->TechnicalAbort(TEXT("Visible queue recovery could not start its native navigation owner."));
		CompleteSoftInterruptionResume();
		return true;
	}

	bQueueRecoveryPending = true;
	if (!QueueNavigation->BeginQueuePoseRecovery(
		FOnCustomerQueueRecoveryFinished::CreateUObject(
			this,
			&UCustomerRoutineInterruptionComponent::HandleQueueRecoveryFinished)))
	{
		bQueueRecoveryPending = false;
		Session->TechnicalAbort(TEXT("Visible queue recovery failed to start."));
		CompleteSoftInterruptionResume();
	}
	return true;
}

void UCustomerRoutineInterruptionComponent::CompleteSoftInterruptionResume()
{
	if (!bSoftInterrupted)
	{
		return;
	}
	if (const ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner()))
	{
		if (UCustomerSessionComponent* Session = Customer->GetCustomerSession())
		{
			Session->ResumeRoutineTimers();
		}
	}
	bQueueRecoveryPending = false;
	bSoftInterrupted = false;
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (const AAIController* Controller = Cast<AAIController>(Pawn->GetController()))
		{
			if (UBrainComponent* Brain = Controller->GetBrainComponent())
			{
				Brain->ResumeLogic(TEXT("Customer recovered from knockdown"));
			}
		}
	}
}

void UCustomerRoutineInterruptionComponent::HandleQueueRecoveryFinished(const bool bSucceeded)
{
	if (!bSoftInterrupted || !bQueueRecoveryPending)
	{
		return;
	}
	if (!bSucceeded)
	{
		if (const ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner()))
		{
			if (UCustomerSessionComponent* Session = Customer->GetCustomerSession(); Session && !Session->IsFinished())
			{
				Session->TechnicalAbort(TEXT("Queue pose recovery exhausted navigation retries."));
			}
		}
	}
	CompleteSoftInterruptionResume();
}

uint64 UCustomerRoutineInterruptionComponent::RegisterRestartableOperation()
{
	ActiveOperationToken = AllocateNonZeroToken(NextOperationToken);
	return ActiveOperationToken;
}

void UCustomerRoutineInterruptionComponent::ClearRestartableOperation(const uint64 OperationToken)
{
	if (OperationToken != 0 && OperationToken == ActiveOperationToken)
	{
		ActiveOperationToken = 0;
	}
}

bool UCustomerRoutineInterruptionComponent::IsRestartableOperationCurrent(const uint64 OperationToken) const
{
	return OperationToken != 0 && OperationToken == ActiveOperationToken;
}

uint64 UCustomerRoutineInterruptionComponent::AllocateNonZeroToken(uint64& Counter)
{
	uint64 Token = ++Counter;
	if (Token == 0)
	{
		Token = ++Counter;
	}
	return Token;
}
