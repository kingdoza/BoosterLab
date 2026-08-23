#include "Customer/CustomerRoutineInterruptionComponent.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Customer/BathhouseCustomerCharacter.h"
#include "Customer/CustomerSessionComponent.h"

UCustomerRoutineInterruptionComponent::UCustomerRoutineInterruptionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCustomerRoutineInterruptionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveOperationToken = 0;
	bSoftInterrupted = false;
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
	if (!bSoftInterrupted)
	{
		return false;
	}
	if (const ABathhouseCustomerCharacter* Customer = Cast<ABathhouseCustomerCharacter>(GetOwner()))
	{
		if (UCustomerSessionComponent* Session = Customer->GetCustomerSession())
		{
			Session->ResumeRoutineTimers();
		}
	}
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
	return true;
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
