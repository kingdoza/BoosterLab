#include "Facility/BathhouseFacilityActor.h"

#include "Components/SceneComponent.h"
#include "Facility/BathhouseFacilitySlotComponent.h"
#include "Facility/BathhouseFacilitySubsystem.h"

ABathhouseFacilityActor::ABathhouseFacilityActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ABathhouseFacilityActor::BeginPlay()
{
	Super::BeginPlay();

	GetComponents<UBathhouseFacilitySlotComponent>(FacilitySlots);
	for (UBathhouseFacilitySlotComponent* Slot : FacilitySlots)
	{
		if (Slot)
		{
			Slot->OnSlotStateChanged.AddDynamic(this, &ABathhouseFacilityActor::HandleSlotStateChanged);
		}
	}

	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>())
	{
		Subsystem->RegisterFacility(this);
	}
}

void ABathhouseFacilityActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr)
	{
		Subsystem->UnregisterFacility(this);
	}

	for (UBathhouseFacilitySlotComponent* Slot : FacilitySlots)
	{
		if (Slot)
		{
			Slot->OnSlotStateChanged.RemoveDynamic(this, &ABathhouseFacilityActor::HandleSlotStateChanged);
			Slot->ForceRelease();
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ABathhouseFacilityActor::HandleSlotStateChanged(
	UBathhouseFacilitySlotComponent* Slot,
	const EBathhouseFacilitySlotState PreviousState,
	const EBathhouseFacilitySlotState NewState)
{
	OnSlotReservationChanged(Slot, NewState);

	if (NewState == EBathhouseFacilitySlotState::Occupied)
	{
		OnSlotUseStarted(Slot, Slot ? Slot->GetCurrentUser() : nullptr);
	}
	else if (PreviousState == EBathhouseFacilitySlotState::Occupied)
	{
		OnSlotUseEnded(Slot, Slot ? Slot->GetCurrentUser() : nullptr);
	}

	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>())
	{
		Subsystem->NotifyFacilityAvailabilityChanged(FacilityType);
	}
}
