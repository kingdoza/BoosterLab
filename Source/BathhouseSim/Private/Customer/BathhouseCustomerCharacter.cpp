#include "Customer/BathhouseCustomerCharacter.h"

#include "Customer/BathhouseCustomerAIController.h"
#include "Customer/CustomerMontagePlaybackComponent.h"
#include "Combat/HealthComponent.h"
#include "Customer/CustomerKnockdownComponent.h"
#include "Customer/CustomerRoutineInterruptionComponent.h"
#include "Customer/CustomerQueueNavigationComponent.h"
#include "Customer/CustomerSessionComponent.h"
#include "Customer/CustomerRoutineDefinition.h"
#include "Facility/BathhouseCounterActor.h"

ABathhouseCustomerCharacter::ABathhouseCustomerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	CustomerSession = CreateDefaultSubobject<UCustomerSessionComponent>(TEXT("CustomerSession"));
	CustomerMontagePlayback = CreateDefaultSubobject<UCustomerMontagePlaybackComponent>(TEXT("CustomerMontagePlayback"));
	Health = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	CustomerKnockdown = CreateDefaultSubobject<UCustomerKnockdownComponent>(TEXT("CustomerKnockdown"));
	CustomerRoutineInterruption = CreateDefaultSubobject<UCustomerRoutineInterruptionComponent>(TEXT("CustomerRoutineInterruption"));
	CustomerQueueNavigation = CreateDefaultSubobject<UCustomerQueueNavigationComponent>(TEXT("CustomerQueueNavigation"));
	AIControllerClass = ABathhouseCustomerAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ABathhouseCustomerCharacter::BeginPlay()
{
	Super::BeginPlay();
	CustomerSession->InitializeSession(RoutineDefinition, Counter);
	NotifyPresentationState(EBathhouseCustomerPresentationState::Entering);
}

void ABathhouseCustomerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnCustomerFinished.Clear();
	Super::EndPlay(EndPlayReason);
}

FPlayerInteractionQuery ABathhouseCustomerCharacter::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	if (CustomerKnockdown && CustomerKnockdown->IsKnockedDown())
	{
		return FPlayerInteractionQuery();
	}
	return CustomerSession ? CustomerSession->QueryCheckInInteraction(Context) : FPlayerInteractionQuery();
}

FPlayerInteractionResult ABathhouseCustomerCharacter::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	if (CustomerKnockdown && CustomerKnockdown->IsKnockedDown())
	{
		return FPlayerInteractionResult::Failed(
			NSLOCTEXT("BathhouseCustomer", "KnockedDownInteraction", "쓰러진 손님과는 상호작용할 수 없습니다."));
	}
	return CustomerSession
		? CustomerSession->ExecuteCheckInInteraction(Context)
		: FPlayerInteractionResult::Failed(NSLOCTEXT("BathhouseCustomer", "MissingSession", "손님 상태를 확인할 수 없습니다."));
}

void ABathhouseCustomerCharacter::InitializeCustomer(
	UCustomerRoutineDefinition* InRoutineDefinition,
	ABathhouseCounterActor* InCounter)
{
	RoutineDefinition = InRoutineDefinition;
	Counter = InCounter;
	if (CustomerSession)
	{
		CustomerSession->InitializeSession(RoutineDefinition, Counter);
	}
}

void ABathhouseCustomerCharacter::NotifyActivityStarted(const EBathhouseCustomerActivity Activity)
{
	OnActivityStarted(Activity);
}

void ABathhouseCustomerCharacter::NotifyActivityFinished(const EBathhouseCustomerActivity Activity)
{
	OnActivityFinished(Activity);
}

void ABathhouseCustomerCharacter::NotifyPresentationState(const EBathhouseCustomerPresentationState PresentationState)
{
	OnCustomerPresentationStateChanged(PresentationState);
}

void ABathhouseCustomerCharacter::NotifySatisfactionChanged(
	const float PreviousSatisfaction,
	const float NewSatisfaction)
{
	OnCustomerSatisfactionChanged(PreviousSatisfaction, NewSatisfaction);
}

void ABathhouseCustomerCharacter::NotifyCustomerFinished(const EBathhouseCustomerDepartureReason Reason)
{
	if (bFinishBroadcast)
	{
		return;
	}
	bFinishBroadcast = true;
	OnCustomerFinished.Broadcast(this, Reason);
}
