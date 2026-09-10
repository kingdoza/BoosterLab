#include "Tests/FacilityPlacementAutomationTestProbe.h"

#include "Components/BoxComponent.h"
#include "Facility/LockerCapacitySubsystem.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"

AFacilityPlacementAutomationActor::AFacilityPlacementAutomationActor()
{
	FacilityType = EBathhouseFacilityType::Shower;
}

void AFacilityPlacementAutomationActor::ConfigureForTest(
	UFacilityPlacementDefinition& InDefinition,
	const EBathhouseFacilityType InType,
	const int32 InFacilityNumber)
{
	FacilityPlacement->PrepareForStagedPlacement(InDefinition);
	FacilityPlacement->CommitStagedPlacement();
	FacilityType = InType;
	FacilityNumber = InFacilityNumber;
}

void AFacilityPlacementAutomationActor::SetInstanceScaleKeepingUnitFootprint(
	const FVector& InScale)
{
	SetActorScale3D(InScale);
	PlacementFootprint->SetRelativeScale3D(FVector(
		FMath::IsNearlyZero(InScale.X) ? 1.0f : 1.0f / InScale.X,
		FMath::IsNearlyZero(InScale.Y) ? 1.0f : 1.0f / InScale.Y,
		FMath::IsNearlyZero(InScale.Z) ? 1.0f : 1.0f / InScale.Z));
}

AFacilityPlacementScaleAutomationActor::AFacilityPlacementScaleAutomationActor()
{
	SetActorScale3D(FVector(2.0f, 2.0f, 1.5f));
}

UFacilityPlacementLockerSlotAutomationComponent::UFacilityPlacementLockerSlotAutomationComponent()
{
	LockerSlotId = TEXT("AutomationLockerSlot");
}

AFacilityPlacementLockerAutomationActor::AFacilityPlacementLockerAutomationActor()
{
	FacilityType = EBathhouseFacilityType::ClothesLocker;
	UFacilityPlacementLockerSlotAutomationComponent* Slot =
		CreateDefaultSubobject<UFacilityPlacementLockerSlotAutomationComponent>(TEXT("AutomationLockerSlot"));
	Slot->SetupAttachment(GetRootComponent());
}

void UFacilityPlacementEventAutomationProbe::Bind(
	UPlayerCarryComponent* InCarry,
	ULockerCapacitySubsystem* InLockers)
{
	Unbind();
	Carry = InCarry;
	Lockers = InLockers;
	if (Carry)
	{
		Carry->OnHeldObjectChanged.AddDynamic(
			this,
			&UFacilityPlacementEventAutomationProbe::HandleHeldChanged);
	}
	if (Lockers)
	{
		Lockers->OnLockerCapacityChanged.AddDynamic(
			this,
			&UFacilityPlacementEventAutomationProbe::HandleCapacityChanged);
	}
}

void UFacilityPlacementEventAutomationProbe::Unbind()
{
	if (IsValid(Carry))
	{
		Carry->OnHeldObjectChanged.RemoveDynamic(
			this,
			&UFacilityPlacementEventAutomationProbe::HandleHeldChanged);
	}
	if (IsValid(Lockers))
	{
		Lockers->OnLockerCapacityChanged.RemoveDynamic(
			this,
			&UFacilityPlacementEventAutomationProbe::HandleCapacityChanged);
	}
	Carry = nullptr;
	Lockers = nullptr;
}

void UFacilityPlacementEventAutomationProbe::ResetCounts()
{
	HeldChangeCount = 0;
	CapacityChangeCount = 0;
}

void UFacilityPlacementEventAutomationProbe::HandleHeldChanged(AActor* HeldActor)
{
	(void)HeldActor;
	++HeldChangeCount;
}

void UFacilityPlacementEventAutomationProbe::HandleCapacityChanged(
	const int32 InstalledCapacity,
	const int32 ActiveLeaseCount)
{
	(void)InstalledCapacity;
	(void)ActiveLeaseCount;
	++CapacityChangeCount;
}
