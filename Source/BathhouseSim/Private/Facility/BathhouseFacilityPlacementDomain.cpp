#include "Facility/BathhouseFacilityActor.h"

#include "Facility/BathhouseFacilitySubsystem.h"
#include "Facility/LockerActionSlotComponent.h"
#include "Facility/LockerCapacitySubsystem.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"

#define LOCTEXT_NAMESPACE "BathhouseFacilityPlacementDomain"

bool ABathhouseFacilityActor::ValidatePlacedDomain(FText& OutFailureReason) const
{
	UBathhouseFacilitySubsystem* Facilities = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr;
	if (!Facilities)
	{
		OutFailureReason = LOCTEXT("MissingFacilitySubsystem", "설비 등록 시스템을 찾을 수 없습니다.");
		return false;
	}
	if (FacilityType == EBathhouseFacilityType::ClothesLocker)
	{
		TArray<ULockerActionSlotComponent*> LockerSlots;
		GetComponents(LockerSlots);
		ULockerCapacitySubsystem* Lockers = GetWorld()->GetSubsystem<ULockerCapacitySubsystem>();
		const int32 Expected = FacilityPlacement && FacilityPlacement->GetDefinition()
			? FacilityPlacement->GetDefinition()->LockerSlotCount : 0;
		if (!Lockers || !Lockers->ValidateLockerBankRegistration(this, LockerSlots, Expected, OutFailureReason))
		{
			return false;
		}
	}
	return true;
}

bool ABathhouseFacilityActor::RegisterPlacedDomain(FText& OutFailureReason, const bool bPublish)
{
	if (bPlacedDomainRegistered) return true;
	if (!ValidatePlacedDomain(OutFailureReason)) return false;
	UBathhouseFacilitySubsystem* Facilities = GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>();
	if (!Facilities->RegisterFacility(this, false))
	{
		OutFailureReason = LOCTEXT("FacilityRegistrationFailed", "설비 등록 상태를 적용할 수 없습니다.");
		return false;
	}
	ULockerCapacitySubsystem* Lockers = nullptr;
	if (FacilityType == EBathhouseFacilityType::ClothesLocker)
	{
		TArray<ULockerActionSlotComponent*> LockerSlots;
		GetComponents(LockerSlots);
		Lockers = GetWorld()->GetSubsystem<ULockerCapacitySubsystem>();
		const int32 Expected = FacilityPlacement && FacilityPlacement->GetDefinition()
			? FacilityPlacement->GetDefinition()->LockerSlotCount : 0;
		if (!Lockers || !Lockers->RegisterLockerBank(this, LockerSlots, Expected, OutFailureReason, false))
		{
			Facilities->UnregisterFacility(this, false);
			return false;
		}
	}
	bPlacedDomainRegistered = true;
	if (bPublish)
	{
		TWeakObjectPtr<ABathhouseFacilityActor> Self(this);
		Facilities->NotifyFacilityAvailabilityChanged(FacilityType);
		if (Self.IsValid() && !bEndingPlay && Lockers)
		{
			Lockers->PublishCapacityMutation();
		}
	}
	return true;
}

void ABathhouseFacilityActor::UnregisterPlacedDomain(
	const bool bUnexpectedEndPlay,
	const bool bPublish)
{
	if (!bPlacedDomainRegistered) return;
	bPlacedDomainRegistered = false;
	UBathhouseFacilitySubsystem* Facilities = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr;
	ULockerCapacitySubsystem* Lockers = nullptr;
	if (FacilityType == EBathhouseFacilityType::ClothesLocker)
	{
		Lockers = GetWorld() ? GetWorld()->GetSubsystem<ULockerCapacitySubsystem>() : nullptr;
		if (Lockers)
		{
			Lockers->UnregisterLockerBank(this, bUnexpectedEndPlay, false);
		}
	}
	if (Facilities)
	{
		Facilities->UnregisterFacility(this, false);
	}
	if (bPublish)
	{
		if (Facilities)
		{
			Facilities->NotifyFacilityAvailabilityChanged(FacilityType);
		}
		if (Lockers)
		{
			Lockers->PublishCapacityMutation();
		}
	}
}

void ABathhouseFacilityActor::HandleExpansionAuthorityChanged(ABathhouseExpansionAuthority* Authority)
{
	(void)Authority;
	if (!bEndingPlay && !bPlacedDomainRegistered && FacilityPlacement
		&& FacilityPlacement->GetMode() == EPlaceableFacilityMode::Placed)
	{
		FText FailureReason;
		RegisterPlacedDomain(FailureReason);
	}
}

#undef LOCTEXT_NAMESPACE
