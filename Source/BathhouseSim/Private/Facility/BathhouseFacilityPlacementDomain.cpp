#include "Facility/BathhouseFacilityActor.h"

#include "Facility/BathWaterStateComponent.h"
#include "Facility/BathhouseFacilitySubsystem.h"
#include "Facility/BathhouseFacilityPlacementInstanceData.h"
#include "Facility/LockerActionSlotComponent.h"
#include "Facility/LockerCapacitySubsystem.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/PlaceableFacilityItemActor.h"

#define LOCTEXT_NAMESPACE "BathhouseFacilityPlacementDomain"

bool ABathhouseFacilityActor::ExportPlacementPayload(
	APlaceableFacilityItemActor& Item,
	FFacilityPlacementPayload& OutPayload,
	FText& OutFailureReason) const
{
	if (!FacilityPlacement || !FacilityPlacement->GetDefinition())
	{
		OutFailureReason = LOCTEXT("MissingExportDefinition", "설비 변환 정의를 찾을 수 없습니다.");
		return false;
	}
	UBathhouseFacilityPlacementInstanceData* Data =
		NewObject<UBathhouseFacilityPlacementInstanceData>(&Item);
	if (!Data)
	{
		OutFailureReason = LOCTEXT("FacilityPayloadAllocationFailed", "설비 변환 데이터를 생성할 수 없습니다.");
		return false;
	}
	Data->FacilityType = FacilityType;
	Data->FacilityNumber = FacilityNumber;
	Data->SelectionWeight = SelectionWeight;
	Data->bEnabled = bEnabled;
	OutPayload.Definition = FacilityPlacement->GetDefinition();
	OutPayload.InstanceData = Data;
	return OutPayload.Validate(Item, OutFailureReason);
}

bool ABathhouseFacilityActor::ImportPlacementPayload(
	const APlaceableFacilityItemActor& Item,
	const FFacilityPlacementPayload& Payload,
	FText& OutFailureReason)
{
	const UBathhouseFacilityPlacementInstanceData* Data =
		Cast<UBathhouseFacilityPlacementInstanceData>(Payload.InstanceData);
	if (!Data || !Payload.Validate(Item, OutFailureReason)
		|| !FacilityPlacement || !FacilityPlacement->IsStagedPlacement()
		|| FacilityPlacement->GetDefinition() != Payload.Definition
		|| Payload.Definition->PlacedFacilityClass.Get() != GetClass()
		|| !StaticEnum<EBathhouseFacilityType>()->IsValidEnumValue(static_cast<int64>(Data->FacilityType))
		|| Data->FacilityNumber < INDEX_NONE || !FMath::IsFinite(Data->SelectionWeight)
		|| Data->SelectionWeight < 0.0f)
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = LOCTEXT("InvalidFacilityPayload", "목욕탕 설비 변환 데이터가 올바르지 않습니다.");
		}
		return false;
	}
	FacilityType = Data->FacilityType;
	FacilityNumber = Data->FacilityNumber;
	SelectionWeight = Data->SelectionWeight;
	bEnabled = Data->bEnabled;
	if (BathWaterState)
	{
		BathWaterState->ResetEmptyForPlacement();
	}
	return true;
}

bool ABathhouseFacilityActor::StagePlacedDomainRegistration(FText& OutFailureReason)
{
	if (!FacilityPlacement || !FacilityPlacement->IsStagedPlacement()
		|| !RegisterPlacedDomain(OutFailureReason, false))
	{
		return false;
	}
	FacilityPlacement->CommitStagedPlacement();
	return true;
}

void ABathhouseFacilityActor::RollbackPlacedDomainRegistration()
{
	UnregisterPlacedDomain(false, false);
	if (FacilityPlacement)
	{
		FacilityPlacement->SetPlacedDomainActive(false);
	}
}

bool ABathhouseFacilityActor::StagePlacedDomainUnregistration(
	FFacilityPlacementPublication& OutPublication,
	FText& OutFailureReason)
{
	if (!FacilityPlacement || FacilityPlacement->IsStagedPlacement()
		|| !FacilityPlacement->IsPlacedDomainActive() || !bPlacedDomainRegistered)
	{
		OutFailureReason = LOCTEXT("FacilityDomainNotPlaced", "설비 domain이 설치 상태로 등록되어 있지 않습니다.");
		return false;
	}
	TWeakObjectPtr<UBathhouseFacilitySubsystem> Facilities(
		GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr);
	TWeakObjectPtr<ULockerCapacitySubsystem> Lockers(
		FacilityType == EBathhouseFacilityType::ClothesLocker && GetWorld()
			? GetWorld()->GetSubsystem<ULockerCapacitySubsystem>()
			: nullptr);
	const EBathhouseFacilityType PublishedType = FacilityType;
	OutPublication.Callback = [Facilities, Lockers, PublishedType]()
	{
		if (Facilities.IsValid())
		{
			Facilities->NotifyFacilityAvailabilityChanged(PublishedType);
		}
		if (Lockers.IsValid())
		{
			Lockers->PublishCapacityMutation();
		}
	};
	UnregisterPlacedDomain(false, false);
	FacilityPlacement->SetPlacedDomainActive(false);
	return true;
}

bool ABathhouseFacilityActor::RollbackPlacedDomainUnregistration(FText& OutFailureReason)
{
	if (!FacilityPlacement || bEndingPlay)
	{
		OutFailureReason = LOCTEXT("FacilityRollbackUnavailable", "설비 domain 등록을 복구할 수 없습니다.");
		return false;
	}
	FacilityPlacement->SetPlacedDomainActive(true);
	if (!RegisterPlacedDomain(OutFailureReason, false))
	{
		FacilityPlacement->SetPlacedDomainActive(false);
		return false;
	}
	return true;
}

void ABathhouseFacilityActor::PublishPlacedDomainRegistration()
{
	UWorld* World = GetWorld();
	const EBathhouseFacilityType PublishedFacilityType = FacilityType;
	TWeakObjectPtr<ABathhouseFacilityActor> Self(this);
	TWeakObjectPtr<UBathhouseFacilitySubsystem> Facilities(
		World ? World->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr);
	TWeakObjectPtr<ULockerCapacitySubsystem> Lockers(
		FacilityType == EBathhouseFacilityType::ClothesLocker && World
			? World->GetSubsystem<ULockerCapacitySubsystem>()
			: nullptr);

	if (Facilities.IsValid())
	{
		Facilities->NotifyFacilityAvailabilityChanged(PublishedFacilityType);
	}
	if (!Self.IsValid())
	{
		if (Facilities.IsValid()
			&& Facilities->CompactInvalidFacilityRegistrations())
		{
			Facilities->NotifyFacilityAvailabilityChanged(PublishedFacilityType);
		}
		if (Lockers.IsValid())
		{
			Lockers->CompactInvalidEntries();
		}
		return;
	}
	if (Lockers.IsValid())
	{
		Lockers->PublishCapacityMutation();
	}
}

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
	if (FacilityPlacement && !FacilityPlacement->IsStagedPlacement())
	{
		FacilityPlacement->SetPlacedDomainActive(true);
	}
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
	bool bLockerRemoved = false;
	if (FacilityType == EBathhouseFacilityType::ClothesLocker)
	{
		Lockers = GetWorld() ? GetWorld()->GetSubsystem<ULockerCapacitySubsystem>() : nullptr;
		if (Lockers)
		{
			bLockerRemoved = Lockers->UnregisterLockerBank(this, bUnexpectedEndPlay, false);
		}
	}
	bool bFacilityRemoved = false;
	if (Facilities)
	{
		bFacilityRemoved = Facilities->UnregisterFacility(this, false);
	}
	if (bPublish)
	{
		if (Facilities && bFacilityRemoved)
		{
			Facilities->NotifyFacilityAvailabilityChanged(FacilityType);
		}
		if (Lockers && bLockerRemoved)
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
