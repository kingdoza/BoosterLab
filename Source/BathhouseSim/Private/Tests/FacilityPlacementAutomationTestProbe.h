#pragma once

#include "CoreMinimal.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Facility/BathhouseExpansionAuthority.h"
#include "Facility/LockerActionSlotComponent.h"
#include "Placement/FacilityPlacementPayload.h"
#include "Placement/FacilityPlacementZoneActor.h"
#include "FacilityPlacementAutomationTestProbe.generated.h"

class UFacilityPlacementDefinition;
class UBathhouseExpansionDefinition;
class ULockerCapacitySubsystem;
class UPlayerCarryComponent;

USTRUCT()
struct FFacilityPlacementNestedReferenceTestData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> Actor = nullptr;
};

UCLASS(Transient, NotBlueprintable)
class UFacilityPlacementReferenceTestData final : public UFacilityPlacementInstanceData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<AActor> DirectActor = nullptr;

	UPROPERTY()
	FFacilityPlacementNestedReferenceTestData Nested;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> ActorArray;

	UPROPERTY()
	TSet<TObjectPtr<AActor>> ActorSet;

	UPROPERTY()
	TMap<FName, TObjectPtr<UActorComponent>> ComponentMap;
};

UCLASS(Transient, NotBlueprintable)
class AFacilityPlacementAutomationActor : public ABathhouseFacilityActor
{
	GENERATED_BODY()

public:
	AFacilityPlacementAutomationActor();
	void ConfigureForTest(
		UFacilityPlacementDefinition& InDefinition,
		EBathhouseFacilityType InType = EBathhouseFacilityType::Shower,
		int32 InFacilityNumber = INDEX_NONE);
	void SetInstanceScaleKeepingUnitFootprint(const FVector& InScale);
};

UCLASS(Transient, NotBlueprintable)
class AFacilityPlacementScaleAutomationActor final : public AFacilityPlacementAutomationActor
{
	GENERATED_BODY()

public:
	AFacilityPlacementScaleAutomationActor();
};

UCLASS(Transient, NotBlueprintable)
class UFacilityPlacementLockerSlotAutomationComponent final : public ULockerActionSlotComponent
{
	GENERATED_BODY()

public:
	UFacilityPlacementLockerSlotAutomationComponent();
};

UCLASS(Transient, NotBlueprintable)
class AFacilityPlacementLockerAutomationActor final : public AFacilityPlacementAutomationActor
{
	GENERATED_BODY()

public:
	AFacilityPlacementLockerAutomationActor();
};

UCLASS(Transient, NotBlueprintable)
class AFacilityPlacementZoneAutomationActor final : public AFacilityPlacementZoneActor
{
	GENERATED_BODY()

public:
	void AddAllowedTag(const FGameplayTag& Tag) { AllowedFacilityTags.AddTag(Tag); }
};

UCLASS(Transient, NotBlueprintable)
class AFacilityPlacementExpansionAutomationAuthority final : public ABathhouseExpansionAuthority
{
	GENERATED_BODY()

public:
	void ConfigureForTest(UBathhouseExpansionDefinition& InDefinition)
	{
		ExpansionDefinition = &InDefinition;
		InitialTierIndex = 0;
	}
};

UCLASS(Transient, NotBlueprintable)
class UFacilityPlacementEventAutomationProbe final : public UObject
{
	GENERATED_BODY()

public:
	void Bind(UPlayerCarryComponent* InCarry, ULockerCapacitySubsystem* InLockers);
	void Unbind();
	void ResetCounts();

	int32 HeldChangeCount = 0;
	int32 CapacityChangeCount = 0;

private:
	UFUNCTION()
	void HandleHeldChanged(AActor* HeldActor);

	UFUNCTION()
	void HandleCapacityChanged(int32 InstalledCapacity, int32 ActiveLeaseCount);

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> Carry = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ULockerCapacitySubsystem> Lockers = nullptr;
};
