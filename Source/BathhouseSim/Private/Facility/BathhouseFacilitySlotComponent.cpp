#include "Facility/BathhouseFacilitySlotComponent.h"

#include "GameFramework/Actor.h"

UBathhouseFacilitySlotComponent::UBathhouseFacilitySlotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UBathhouseFacilitySlotComponent::TryReserve(AActor* Requestor)
{
	if (!IsValid(Requestor) || !bEnabled)
	{
		return false;
	}

	if (SlotState == EBathhouseFacilitySlotState::Reserved && ReservationOwner == Requestor)
	{
		return true;
	}

	if (SlotState != EBathhouseFacilitySlotState::Available)
	{
		return false;
	}

	SetState(EBathhouseFacilitySlotState::Reserved, Requestor, nullptr);
	return true;
}

bool UBathhouseFacilitySlotComponent::BeginUse(AActor* Requestor)
{
	if (!IsValid(Requestor))
	{
		return false;
	}

	if (SlotState == EBathhouseFacilitySlotState::Occupied && Occupant == Requestor)
	{
		return true;
	}

	if (SlotState != EBathhouseFacilitySlotState::Reserved || ReservationOwner != Requestor)
	{
		return false;
	}

	SetState(EBathhouseFacilitySlotState::Occupied, Requestor, Requestor);
	return true;
}

bool UBathhouseFacilitySlotComponent::EndUse(AActor* Requestor)
{
	if (!IsValid(Requestor))
	{
		return false;
	}

	if (SlotState == EBathhouseFacilitySlotState::Reserved && ReservationOwner == Requestor)
	{
		return true;
	}

	if (SlotState != EBathhouseFacilitySlotState::Occupied || Occupant != Requestor)
	{
		return false;
	}

	SetState(EBathhouseFacilitySlotState::Reserved, Requestor, nullptr);
	return true;
}

bool UBathhouseFacilitySlotComponent::Release(AActor* Requestor)
{
	if (!IsValid(Requestor))
	{
		return false;
	}

	if (SlotState == EBathhouseFacilitySlotState::Available)
	{
		return true;
	}

	if (ReservationOwner != Requestor && Occupant != Requestor)
	{
		return false;
	}

	SetState(EBathhouseFacilitySlotState::Available, nullptr, nullptr);
	return true;
}

void UBathhouseFacilitySlotComponent::ForceRelease()
{
	if (SlotState != EBathhouseFacilitySlotState::Available)
	{
		SetState(EBathhouseFacilitySlotState::Available, nullptr, nullptr);
	}
}

AActor* UBathhouseFacilitySlotComponent::GetCurrentUser() const
{
	return SlotState == EBathhouseFacilitySlotState::Occupied ? Occupant.Get() : ReservationOwner.Get();
}

FTransform UBathhouseFacilitySlotComponent::GetActionTransform() const
{
	FTransform Result = GetComponentTransform();
	Result.SetRotation(Result.TransformRotation(FacingRotation.Quaternion()));
	return Result;
}

FTransform UBathhouseFacilitySlotComponent::GetApproachTransform() const
{
	const FTransform ComponentTransform = GetComponentTransform();
	FTransform Result = GetActionTransform();
	Result.SetLocation(ComponentTransform.TransformPosition(ApproachOffset));
	return Result;
}

void UBathhouseFacilitySlotComponent::SetState(
	const EBathhouseFacilitySlotState NewState,
	AActor* NewReservation,
	AActor* NewOccupant)
{
	const EBathhouseFacilitySlotState PreviousState = SlotState;
	SlotState = NewState;
	ReservationOwner = NewReservation;
	Occupant = NewOccupant;

	if (PreviousState != NewState)
	{
		OnSlotStateChanged.Broadcast(this, PreviousState, NewState);
	}
}
