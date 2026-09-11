#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/PlaceableFacility.h"
#include "Placement/PlaceableFacilityItemActor.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UFacilityPlacementDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("FacilityPlacementDefinition"), StableId.IsNone() ? GetFName() : StableId);
}

bool UFacilityPlacementDefinition::ValidateRuntime(FText& OutFailureReason) const
{
	if (StableId.IsNone() || FacilityTags.IsEmpty() || LockerSlotCount < 0)
	{
		OutFailureReason = NSLOCTEXT(
			"FacilityPlacementDefinition",
			"InvalidCommonDefinition",
			"설비 배치 정의의 식별자, 태그 또는 락커 수가 올바르지 않습니다.");
		return false;
	}
	if (!PlacedFacilityClass
		|| !PlacedFacilityClass->ImplementsInterface(UPlaceableFacility::StaticClass()))
	{
		OutFailureReason = NSLOCTEXT(
			"FacilityPlacementDefinition",
			"InvalidPlacedClass",
			"배치 설비 클래스가 IPlaceableFacility을 구현하지 않습니다.");
		return false;
	}
	const AActor* PlacedCDO = PlacedFacilityClass->GetDefaultObject<AActor>();
	const IPlaceableFacility* PlaceableCDO = Cast<IPlaceableFacility>(PlacedCDO);
	if (!PlaceableCDO || !PlaceableCDO->SupportsFacilityActorConversion())
	{
		OutFailureReason = NSLOCTEXT(
			"FacilityPlacementDefinition",
			"PlacedClassConversionUnsupported",
			"이 배치 설비 클래스는 Actor 배치 및 회수를 지원하지 않습니다.");
		return false;
	}
	FIntPoint DerivedCells;
	if (!DeriveFootprintCells(DerivedCells, OutFailureReason))
	{
		return false;
	}
	if (!PlaceableCDO->GetFacilityPlacementComponent()->ValidateNavigationContract(OutFailureReason))
	{
		return false;
	}
	if (!RecoveryItemClass
		|| !RecoveryItemClass->IsChildOf(APlaceableFacilityItemActor::StaticClass())
		|| PlacedFacilityClass.Get() == RecoveryItemClass.Get())
	{
		OutFailureReason = NSLOCTEXT(
			"FacilityPlacementDefinition",
			"InvalidRecoveryItemClass",
			"회수 아이템은 공통 설비 아이템 클래스 또는 그 파생 클래스여야 하며 배치 설비와 달라야 합니다.");
		return false;
	}
	if (UStaticMesh* Mesh = APlaceableFacilityItemActor::ResolveRecoveryMesh(*this))
	{
		return APlaceableFacilityItemActor::ValidateRecoveryMesh(*Mesh, OutFailureReason);
	}
	OutFailureReason = NSLOCTEXT(
		"FacilityPlacementDefinition",
		"MissingRecoveryMeshFallback",
		"회수 아이템 메시와 native Cube fallback을 찾을 수 없습니다.");
	return false;
}

bool UFacilityPlacementDefinition::DeriveFootprintCells(
	FIntPoint& OutCells,
	FText& OutFailureReason) const
{
	OutCells = FIntPoint::ZeroValue;
	const AActor* PlacedCDO = PlacedFacilityClass
		? PlacedFacilityClass->GetDefaultObject<AActor>()
		: nullptr;
	const IPlaceableFacility* Placeable = Cast<IPlaceableFacility>(PlacedCDO);
	const UFacilityPlacementComponent* Placement = Placeable
		? Placeable->GetFacilityPlacementComponent()
		: nullptr;
	if (!Placement)
	{
		OutFailureReason = NSLOCTEXT("FacilityPlacementDefinition", "MissingPlacementComponent", "배치 설비 클래스의 FacilityPlacement Component를 찾을 수 없습니다.");
		return false;
	}
	return Placement->DeriveFootprintCells(OutCells, OutFailureReason);
}

#if WITH_EDITOR
EDataValidationResult UFacilityPlacementDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Invalidate = [&](const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	};
	if (StableId.IsNone())
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "MissingStableId", "Stable Id is required."));
	}
	if (FacilityTags.IsEmpty())
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "MissingTags", "At least one facility tag is required."));
	}
	if (!PlacedFacilityClass
		|| !PlacedFacilityClass->ImplementsInterface(UPlaceableFacility::StaticClass()))
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "InvalidPlacedClass", "Placed Facility Class must implement IPlaceableFacility."));
	}
	else
	{
		const AActor* PlacedCDO = PlacedFacilityClass->GetDefaultObject<AActor>();
		const IPlaceableFacility* PlaceableCDO = Cast<IPlaceableFacility>(PlacedCDO);
		if (!PlaceableCDO || !PlaceableCDO->SupportsFacilityActorConversion())
		{
			Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "UnsupportedPlacedClass", "Placed Facility Class is excluded from Actor placement and recovery."));
		}
		else
		{
			FIntPoint DerivedCells;
			FText FootprintFailure;
			if (!DeriveFootprintCells(DerivedCells, FootprintFailure))
			{
				Invalidate(FootprintFailure);
			}
			else if (!PlaceableCDO->GetFacilityPlacementComponent()->ValidateNavigationContract(FootprintFailure))
			{
				Invalidate(FootprintFailure);
			}
		}
	}
	if (!RecoveryItemClass
		|| !RecoveryItemClass->IsChildOf(APlaceableFacilityItemActor::StaticClass()))
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "InvalidRecoveryClass", "Recovery Item Class must derive from APlaceableFacilityItemActor."));
	}
	if (PlacedFacilityClass.Get() == RecoveryItemClass.Get())
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "MatchingConversionClasses", "Placed and recovery item classes must be different."));
	}
	if (RecoveryItemMesh)
	{
		FText MeshFailure;
		if (!APlaceableFacilityItemActor::ValidateRecoveryMesh(*RecoveryItemMesh, MeshFailure))
		{
			Invalidate(MeshFailure);
		}
	}
	else
	{
		Context.AddWarning(NSLOCTEXT("FacilityPlacementDefinition", "CubeFallback", "Recovery Item Mesh is unset; the native Engine Cube fallback will be used."));
	}
	if (LockerSlotCount < 0)
	{
		Invalidate(NSLOCTEXT("FacilityPlacementDefinition", "InvalidLockerCount", "Locker slot count cannot be negative."));
	}
	return Result == EDataValidationResult::NotValidated
		? EDataValidationResult::Valid
		: Result;
}
#endif
