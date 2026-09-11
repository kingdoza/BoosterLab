#include "Tests/FacilityPlacementAutomationTestProbe.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Facility/LockerCapacitySubsystem.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"

AFacilityPlacementItemAutomationActor::AFacilityPlacementItemAutomationActor()
{
	SetActorScale3D(FVector(0.25f));
}

AFacilityPlacementAutomationActor::AFacilityPlacementAutomationActor()
{
	FacilityType = EBathhouseFacilityType::Shower;
	AutomationPreviewBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AutomationPreviewBody"));
	AutomationPreviewBody->SetupAttachment(GetRootComponent());
	AutomationPreviewBodySecondary = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AutomationPreviewBodySecondary"));
	AutomationPreviewBodySecondary->SetupAttachment(AutomationPreviewBody);
	AutomationPreviewBodySecondary->SetRelativeLocation(FVector(20.0f, 0.0f, 0.0f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		AutomationPreviewBody->SetStaticMesh(CubeMesh.Object);
		AutomationPreviewBodySecondary->SetStaticMesh(CubeMesh.Object);
	}
}

void AFacilityPlacementAutomationActor::ConfigureForTest(
	UFacilityPlacementDefinition& InDefinition,
	const EBathhouseFacilityType InType,
	const int32 InFacilityNumber)
{
	FText FailureReason;
	ensure(FacilityPlacement->PrepareForStagedPlacement(InDefinition, FailureReason));
	ensure(FacilityPlacement->FinalizeStagedPlacementCollisionSnapshot(FailureReason));
	ensure(FacilityPlacement->CommitStagedPlacement(FailureReason));
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

AFacilityPlacementConstructionCollisionAutomationActor::
	AFacilityPlacementConstructionCollisionAutomationActor()
{
	SetActorEnableCollision(false);
	AutomationPreviewBody->SetCanEverAffectNavigation(false);
	AutomationPreviewBodySecondary->SetCanEverAffectNavigation(false);
}

void AFacilityPlacementConstructionCollisionAutomationActor::OnConstruction(
	const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetActorEnableCollision(true);
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

void AFacilityPlacementLockerAutomationActor::ConfigureStartupForTest(
	UFacilityPlacementDefinition& InDefinition,
	const FGuid& InRegistrationId,
	const int32 TotalSlots)
{
	ConfigureForTest(InDefinition, EBathhouseFacilityType::ClothesLocker);
	RegistrationId = InRegistrationId;
	bNetStartup = true;
	for (int32 Index = 1; Index < TotalSlots; ++Index)
	{
		UFacilityPlacementLockerSlotAutomationComponent* Slot =
			NewObject<UFacilityPlacementLockerSlotAutomationComponent>(
				this, *FString::Printf(TEXT("AutomationLockerSlot_%d"), Index));
		Slot->SetSlotIdForTest(*FString::Printf(TEXT("AutomationLockerSlot_%d"), Index));
		Slot->SetupAttachment(GetRootComponent());
		AddInstanceComponent(Slot);
	}
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
