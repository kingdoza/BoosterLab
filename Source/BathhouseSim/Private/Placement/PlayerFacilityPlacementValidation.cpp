#include "Placement/PlayerFacilityPlacementComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Placement/FacilityPlacementCollisionUtils.h"
#include "Placement/FacilityPlacementComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementPreviewActor.h"
#include "Placement/FacilityPlacementSettings.h"
#include "Placement/FacilityPlacementZoneActor.h"
#include "Placement/PlaceableFacility.h"
#include "Placement/PlaceableFacilityItemActor.h"
#include "Facility/LockerCapacitySubsystem.h"

#define LOCTEXT_NAMESPACE "PlayerFacilityPlacementValidation"

bool UPlayerFacilityPlacementComponent::IsLocalSessionOwner() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return !OwnerPawn || OwnerPawn->IsLocallyControlled();
}

bool UPlayerFacilityPlacementComponent::CanProcessSession() const
{
	return IsLocalSessionOwner()
		&& (!Interaction || (IsValid(Interaction) && !Interaction->IsInteractionSuppressed()));
}

void UPlayerFacilityPlacementComponent::UpdateTickState()
{
	SetComponentTickEnabled(PreviewActor.IsValid() || RecoveryTarget.IsValid());
}

bool UPlayerFacilityPlacementComponent::TracePlacementZone(
	AFacilityPlacementZoneActor*& OutZone,
	FVector& OutPoint) const
{
	OutZone = nullptr;
	OutPoint = FVector::ZeroVector;
	if (!CanProcessSession() || !Camera || !GetWorld() || !GetOwner()) return false;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FacilityPlacementZoneTrace), true, GetOwner());
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * GetDefault<UFacilityPlacementSettings>()->GetPlacementTraceDistance();
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return false;
	OutZone = Cast<AFacilityPlacementZoneActor>(Hit.GetActor());
	OutPoint = Hit.ImpactPoint;
	return OutZone != nullptr;
}

AActor* UPlayerFacilityPlacementComponent::TraceRecoveryTarget() const
{
	if (!CanProcessSession() || !Camera || !GetWorld() || !GetOwner()) return nullptr;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FacilityRecoveryTrace), true, GetOwner());
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * GetDefault<UFacilityPlacementSettings>()->GetRecoveryTraceDistance();
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return nullptr;
	return Hit.GetActor() && Hit.GetActor()->GetClass()->ImplementsInterface(UPlaceableFacility::StaticClass()) ? Hit.GetActor() : nullptr;
}

FFacilityPlacementTransactionResult UPlayerFacilityPlacementComponent::ValidateCurrentPlacement(
	FTransform& OutCandidate,
	AFacilityPlacementZoneActor*& OutZone) const
{
	APlaceableFacilityItemActor* Item = PreviewFacility.Get();
	UFacilityPlacementDefinition* Definition = Item ? Item->GetDefinition() : nullptr;
	AActor* PlacedCDO = Definition && Definition->PlacedFacilityClass
		? Definition->PlacedFacilityClass->GetDefaultObject<AActor>() : nullptr;
	IPlaceableFacility* Placeable = Cast<IPlaceableFacility>(PlacedCDO);
	UFacilityPlacementComponent* Placement = Placeable ? Placeable->GetFacilityPlacementComponent() : nullptr;
	FVector Point;
	FText FailureReason;
	if (!CanProcessSession() || !IsValid(PreviewActor.Get()) || !Item || !Definition
		|| !Definition->ValidateRuntime(FailureReason) || !PlacedCDO || !Placement
		|| !Carry || Carry->GetHeldObject() != Item || !Item->IsHeldForPlacement())
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::StateChanged, LOCTEXT("HeldFacilityChanged", "들고 있는 설비가 변경되었습니다."));
	if (!PreviewActor->ValidateSourceGeometry(Definition->PlacedFacilityClass, FailureReason))
	{
		return FFacilityPlacementTransactionResult::Failed(
			EFacilityPlacementFailureCode::InvalidFootprint,
			FailureReason);
	}
	if (!TracePlacementZone(OutZone, Point) || !OutZone)
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::NoCompatibleZone, LOCTEXT("NoZone", "설치 가능한 구역을 바라보세요."));
	const FTransform RequestedTransform = OutZone->MakeCandidateTransform(Point, AccumulatedYaw, bSnapHeld);
	if (!Placement->BuildPlacedActorTransform(RequestedTransform, OutCandidate, FailureReason))
	{
		return FFacilityPlacementTransactionResult::Failed(
			EFacilityPlacementFailureCode::InvalidComponents,
			FailureReason);
	}
	if (!OutZone->IsDefinitionAllowed(*Definition))
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::NoCompatibleZone, LOCTEXT("ZoneTagMismatch", "이 구역에는 해당 설비를 설치할 수 없습니다."));
	}
	if (Definition->LockerSlotCount > 0)
	{
		const ULockerCapacitySubsystem* Lockers = GetWorld()->GetSubsystem<ULockerCapacitySubsystem>();
		if (!Lockers || !Lockers->CanInstallLockerSlots(Definition->LockerSlotCount, FailureReason))
		{
			return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::ExpansionLimit, FailureReason);
		}
	}
	return ValidateWorldPlacement(*Item, OutCandidate, *OutZone);
}

FFacilityPlacementTransactionResult UPlayerFacilityPlacementComponent::ValidateWorldPlacement(
	APlaceableFacilityItemActor& Item,
	const FTransform& Candidate,
	const AFacilityPlacementZoneActor& Zone) const
{
	UFacilityPlacementDefinition* Definition = Item.GetDefinition();
	AActor* PlacedCDO = Definition && Definition->PlacedFacilityClass
		? Definition->PlacedFacilityClass->GetDefaultObject<AActor>() : nullptr;
	IPlaceableFacility* Placeable = Cast<IPlaceableFacility>(PlacedCDO);
	UFacilityPlacementComponent* Placement = Placeable ? Placeable->GetFacilityPlacementComponent() : nullptr;
	UBoxComponent* Footprint = Placement ? Placement->GetPlacementFootprint() : nullptr;
	FText FailureReason;
	if (!Definition || !PlacedCDO || !Placement || !Footprint
		|| !Placement->ValidateFootprintContractForDefinition(*Definition, FailureReason))
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::InvalidFootprint, FailureReason);
	FTransform RelativeFootprint;
	if (!Placement->GetFootprintRelativeToRoot(RelativeFootprint, FailureReason))
	{
		return FFacilityPlacementTransactionResult::Failed(
			EFacilityPlacementFailureCode::InvalidFootprint,
			FailureReason);
	}
	const FTransform FootprintTransform = RelativeFootprint * Candidate;
	const FVector HalfExtent = Footprint->GetUnscaledBoxExtent() * FootprintTransform.GetScale3D().GetAbs();
	if (!Zone.ContainsFootprint(FootprintTransform, HalfExtent))
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::OutsideZone, LOCTEXT("OutsideZone", "설비 전체가 하나의 설치 구역 안에 있어야 합니다."));

	UPrimitiveComponent* ItemRoot = Item.GetItemRoot();
	if (!IsValid(ItemRoot))
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::InvalidComponents, LOCTEXT("MissingItemRoot", "설비 아이템 충돌 설정이 올바르지 않습니다."));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FacilityPlacementOverlap), false, &Item);
	Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(&Zone);
	const FVector QueryExtent(HalfExtent.X * 0.98f, HalfExtent.Y * 0.98f, FMath::Max(1.0f, HalfExtent.Z * 0.9f));
	if (FacilityPlacementCollision::HasBlockingOverlap(
		*GetWorld(),
		FootprintTransform.GetLocation(),
		FootprintTransform.GetRotation(),
		FCollisionShape::MakeBox(QueryExtent),
		*ItemRoot,
		Params))
	{
		return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::Blocked, LOCTEXT("PlacementBlocked", "다른 물체와 겹쳐 설비를 설치할 수 없습니다."));
	}

	FCollisionObjectQueryParams FloorObjects;
	FloorObjects.AddObjectTypesToQuery(ECC_WorldStatic);
	FloorObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
	const FVector FloorNormal = Zone.GetPlacementFloor()
		? Zone.GetPlacementFloor()->GetUpVector()
		: Zone.GetActorUpVector();
	const FVector Forward = FootprintTransform.GetUnitAxis(EAxis::X);
	const FVector Right = FootprintTransform.GetUnitAxis(EAxis::Y);
	const FVector BottomCenter = FootprintTransform.TransformPosition(
		FVector(0.0f, 0.0f, -Footprint->GetUnscaledBoxExtent().Z));
	for (const FVector2D Corner : { FVector2D(-1.0f, -1.0f), FVector2D(-1.0f, 1.0f), FVector2D(1.0f, -1.0f), FVector2D(1.0f, 1.0f) })
	{
		const FVector Sample = BottomCenter
			+ Forward * (Corner.X * HalfExtent.X * 0.9f)
			+ Right * (Corner.Y * HalfExtent.Y * 0.9f);
		FHitResult FloorHit;
		if (!GetWorld()->LineTraceSingleByObjectType(
			FloorHit,
			Sample + FloorNormal * 5.0f,
			Sample - FloorNormal * 25.0f,
			FloorObjects,
			Params))
		{
			return FFacilityPlacementTransactionResult::Failed(EFacilityPlacementFailureCode::NoFloorSupport, LOCTEXT("NoFloor", "설비 전체를 지지할 바닥이 없습니다."));
		}
	}
	return FFacilityPlacementTransactionResult::Succeeded();
}

FPlayerInteractionQuery UPlayerFacilityPlacementComponent::MergeSupplementalInteractionQuery(
	const FPlayerInteractionQuery& BaseQuery) const
{
	FPlayerInteractionQuery Query = BaseQuery;
	if (!CanProcessSession())
	{
		return Query;
	}
	if (PreviewFacility.IsValid())
	{
		Query.bPlacementVisible = true;
		Query.bCanPlace = PreviewActor.IsValid() && CurrentPlacementQuery.bSucceeded;
		Query.PlacementActionName = LOCTEXT("PlaceFacility", "설비 설치");
		Query.PlacementFailureReason = CurrentPlacementQuery.FailureReason;
		Query.bEquipmentUseVisible = false;
		Query.bCanEquipmentUse = false;
		Query.EquipmentActionName = FText::GetEmpty();
		Query.EquipmentFailureReason = FText::GetEmpty();
		Query.EquipmentUseProgress = 0.0f;
	}
	if (RecoveryTarget.IsValid() && Query.bRecoveryVisible)
	{
		Query.RecoveryProgress = FMath::Clamp(
			RecoveryElapsed / GetDefault<UFacilityPlacementSettings>()->GetRecoveryHoldSeconds(),
			0.0f,
			1.0f);
	}
	return Query;
}

#undef LOCTEXT_NAMESPACE
