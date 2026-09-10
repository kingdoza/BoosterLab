#include "Towel/TowelProcessingMachineActor.h"

#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/PlaceableFacilityItemActor.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/TowelMachinePlacementInstanceData.h"

#define LOCTEXT_NAMESPACE "TowelProcessingMachinePlacement"

bool ATowelProcessingMachineActor::ExportPlacementPayload(
	APlaceableFacilityItemActor& Item,
	FFacilityPlacementPayload& OutPayload,
	FText& OutFailureReason) const
{
	if (!FacilityPlacement || !FacilityPlacement->GetDefinition())
	{
		OutFailureReason = LOCTEXT("MissingMachineDefinition", "수건 처리기 변환 정의를 찾을 수 없습니다.");
		return false;
	}
	UTowelMachinePlacementInstanceData* Data = NewObject<UTowelMachinePlacementInstanceData>(&Item);
	if (!Data)
	{
		OutFailureReason = LOCTEXT("MachinePayloadAllocationFailed", "수건 처리기 변환 데이터를 생성할 수 없습니다.");
		return false;
	}
	Data->MachineKind = MachineKind;
	Data->ProcessingDurationSeconds = ProcessingDurationSeconds;
	OutPayload.Definition = FacilityPlacement->GetDefinition();
	OutPayload.InstanceData = Data;
	return OutPayload.Validate(Item, OutFailureReason);
}

bool ATowelProcessingMachineActor::ImportPlacementPayload(
	const APlaceableFacilityItemActor& Item,
	const FFacilityPlacementPayload& Payload,
	FText& OutFailureReason)
{
	const UTowelMachinePlacementInstanceData* Data =
		Cast<UTowelMachinePlacementInstanceData>(Payload.InstanceData);
	if (!Data || !Payload.Validate(Item, OutFailureReason)
		|| !FacilityPlacement || !FacilityPlacement->IsStagedPlacement()
		|| FacilityPlacement->GetDefinition() != Payload.Definition
		|| Payload.Definition->PlacedFacilityClass.Get() != GetClass()
		|| !StaticEnum<ETowelMachineKind>()->IsValidEnumValue(static_cast<int64>(Data->MachineKind))
		|| !FMath::IsFinite(Data->ProcessingDurationSeconds)
		|| Data->ProcessingDurationSeconds < 0.1f)
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = LOCTEXT("InvalidMachinePayload", "수건 처리기 변환 데이터가 올바르지 않습니다.");
		}
		return false;
	}
	MachineKind = Data->MachineKind;
	ProcessingDurationSeconds = Data->ProcessingDurationSeconds;
	MachineState = ETowelMachineState::Waiting;
	ProcessingEndTime = 0.0;
	if (Inventory)
	{
		Inventory->ConfigureDefaults(ETowelState::None, 0, Inventory->GetSnapshot().Capacity);
	}
	return true;
}

bool ATowelProcessingMachineActor::StagePlacedDomainRegistration(FText& OutFailureReason)
{
	if (!FacilityPlacement || !FacilityPlacement->IsStagedPlacement())
	{
		OutFailureReason = LOCTEXT("MachineNotStaged", "새 수건 처리기가 staged 상태가 아닙니다.");
		return false;
	}
	FacilityPlacement->CommitStagedPlacement();
	return true;
}

void ATowelProcessingMachineActor::RollbackPlacedDomainRegistration()
{
	if (FacilityPlacement)
	{
		FacilityPlacement->SetPlacedDomainActive(false);
	}
}

bool ATowelProcessingMachineActor::StagePlacedDomainUnregistration(
	FFacilityPlacementPublication& OutPublication,
	FText& OutFailureReason)
{
	OutPublication.Callback = TFunction<void()>();
	if (!FacilityPlacement || FacilityPlacement->IsStagedPlacement()
		|| !FacilityPlacement->IsPlacedDomainActive())
	{
		OutFailureReason = LOCTEXT("MachineDomainNotPlaced", "수건 처리기가 설치 상태가 아닙니다.");
		return false;
	}
	FacilityPlacement->SetPlacedDomainActive(false);
	return true;
}

bool ATowelProcessingMachineActor::RollbackPlacedDomainUnregistration(FText& OutFailureReason)
{
	if (!FacilityPlacement || IsActorBeingDestroyed())
	{
		OutFailureReason = LOCTEXT("MachineRollbackUnavailable", "수건 처리기 설치 상태를 복구할 수 없습니다.");
		return false;
	}
	FacilityPlacement->SetPlacedDomainActive(true);
	return true;
}

void ATowelProcessingMachineActor::PublishPlacedDomainRegistration()
{
}

#undef LOCTEXT_NAMESPACE
