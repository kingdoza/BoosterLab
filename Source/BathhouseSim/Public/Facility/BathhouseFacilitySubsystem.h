#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "BathhouseFacilitySubsystem.generated.h"

class ABathhouseFacilityActor;
class UBathhouseFacilitySlotComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFacilityAvailabilityChangedNative, EBathhouseFacilityType);
DECLARE_MULTICAST_DELEGATE(FOnKeyTopologyChangedNative);

UCLASS()
class BATHHOUSESIM_API UBathhouseFacilitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterFacility(ABathhouseFacilityActor* Facility);
	void UnregisterFacility(ABathhouseFacilityActor* Facility);
	void NotifyFacilityAvailabilityChanged(EBathhouseFacilityType FacilityType);

	void RegisterKeyHook(AActor* KeyHook, int32 KeyNumber);
	void UnregisterKeyHook(AActor* KeyHook, int32 KeyNumber);

	bool ValidateKeyNumber(int32 KeyNumber, const AActor* ExpectedKeyHook = nullptr, FText* OutFailureReason = nullptr) const;
	ABathhouseFacilityActor* FindNumberedFacility(EBathhouseFacilityType FacilityType, int32 FacilityNumber) const;
	void GetFacilitiesOfType(EBathhouseFacilityType FacilityType, TArray<ABathhouseFacilityActor*>& OutFacilities) const;

	bool TryReserveRandomSlot(
		EBathhouseFacilityType FacilityType,
		AActor* Requestor,
		ABathhouseFacilityActor*& OutFacility,
		UBathhouseFacilitySlotComponent*& OutSlot,
		int32 FacilityNumber = INDEX_NONE,
		const ABathhouseFacilityActor* ExcludedFacility = nullptr) const;

	FOnFacilityAvailabilityChangedNative OnFacilityAvailabilityChanged;
	FOnKeyTopologyChangedNative OnKeyTopologyChanged;

private:
	void CompactRegistrations();

	TArray<TWeakObjectPtr<ABathhouseFacilityActor>> RegisteredFacilities;
	TMultiMap<int32, TWeakObjectPtr<AActor>> RegisteredKeyHooks;
};
