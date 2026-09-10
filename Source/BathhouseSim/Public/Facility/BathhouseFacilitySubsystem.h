#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "BathhouseFacilitySubsystem.generated.h"

class ABathhouseFacilityActor;
class ABathhouseExpansionAuthority;
class UBathhouseFacilitySlotComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnFacilityAvailabilityChangedNative, EBathhouseFacilityType);
DECLARE_MULTICAST_DELEGATE(FOnKeyTopologyChangedNative);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnExpansionAuthorityChangedNative, ABathhouseExpansionAuthority*);

UCLASS()
class BATHHOUSESIM_API UBathhouseFacilitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool RegisterFacility(ABathhouseFacilityActor* Facility, bool bPublish = true);
	bool UnregisterFacility(ABathhouseFacilityActor* Facility, bool bPublish = true);
	bool IsFacilityRegistered(const ABathhouseFacilityActor* Facility) const;
	bool CompactInvalidFacilityRegistrations();
	void NotifyFacilityAvailabilityChanged(EBathhouseFacilityType FacilityType);

	void RegisterKeyHook(AActor* KeyHook, int32 KeyNumber);
	void UnregisterKeyHook(AActor* KeyHook, int32 KeyNumber);

	bool ValidateKeyNumber(int32 KeyNumber, const AActor* ExpectedKeyHook = nullptr, FText* OutFailureReason = nullptr) const;
	ABathhouseFacilityActor* FindNumberedFacility(EBathhouseFacilityType FacilityType, int32 FacilityNumber) const;
	void GetFacilitiesOfType(EBathhouseFacilityType FacilityType, TArray<ABathhouseFacilityActor*>& OutFacilities) const;
	bool RegisterExpansionAuthority(ABathhouseExpansionAuthority* Authority, FText& OutFailureReason);
	void UnregisterExpansionAuthority(ABathhouseExpansionAuthority* Authority);
	ABathhouseExpansionAuthority* GetExpansionAuthority() const { return ExpansionAuthority.Get(); }
	int32 GetMaxInstalledLockerSlots() const;
	int32 GetCurrentKeyPoolSize() const;

	bool TryReserveRandomSlot(
		EBathhouseFacilityType FacilityType,
		AActor* Requestor,
		ABathhouseFacilityActor*& OutFacility,
		UBathhouseFacilitySlotComponent*& OutSlot,
		int32 FacilityNumber = INDEX_NONE,
		const ABathhouseFacilityActor* ExcludedFacility = nullptr) const;

	FOnFacilityAvailabilityChangedNative OnFacilityAvailabilityChanged;
	FOnKeyTopologyChangedNative OnKeyTopologyChanged;
	FOnExpansionAuthorityChangedNative OnExpansionAuthorityChanged;

private:
	void CompactRegistrations();

	TArray<TWeakObjectPtr<ABathhouseFacilityActor>> RegisteredFacilities;
	TMultiMap<int32, TWeakObjectPtr<AActor>> RegisteredKeyHooks;
	TWeakObjectPtr<ABathhouseExpansionAuthority> ExpansionAuthority;
};
