#include "Placement/FacilityPlacementComponent.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementSettings.h"

#define LOCTEXT_NAMESPACE "FacilityPlacementGeometry"

bool UFacilityPlacementComponent::ValidateFootprintContractForDefinition(
	const UFacilityPlacementDefinition& InDefinition,
	FText& OutFailureReason) const
{
	(void)InDefinition;
	FIntPoint Cells;
	return DeriveFootprintCells(Cells, OutFailureReason);
}

bool UFacilityPlacementComponent::DeriveFootprintCells(
	FIntPoint& OutCells,
	FText& OutFailureReason) const
{
	OutCells = FIntPoint::ZeroValue;
	if (!PlacementFootprint)
	{
		OutFailureReason = LOCTEXT("MissingFootprint", "설비 배치 영역을 확인할 수 없습니다.");
		return false;
	}
	const float Grid = GetDefault<UFacilityPlacementSettings>()->GetGridSizeCm();
	const FVector FullSize = PlacementFootprint->GetUnscaledBoxExtent()
		* PlacementFootprint->GetComponentTransform().GetScale3D().GetAbs() * 2.0f;
	const FVector2D Ratios(FullSize.X / Grid, FullSize.Y / Grid);
	const int32 CellsX = FMath::RoundToInt(Ratios.X);
	const int32 CellsY = FMath::RoundToInt(Ratios.Y);
	if (!FMath::IsFinite(FullSize.X) || !FMath::IsFinite(FullSize.Y)
		|| FullSize.X <= UE_KINDA_SMALL_NUMBER || FullSize.Y <= UE_KINDA_SMALL_NUMBER
		|| CellsX < 1 || CellsY < 1
		|| !FMath::IsNearlyEqual(Ratios.X, static_cast<float>(CellsX), 0.01f)
		|| !FMath::IsNearlyEqual(Ratios.Y, static_cast<float>(CellsY), 0.01f))
	{
		OutFailureReason = LOCTEXT("FootprintGridMismatch", "설비 배치 영역이 정의의 전역 그리드 셀 크기와 일치하지 않습니다.");
		return false;
	}
	OutCells = FIntPoint(CellsX, CellsY);
	return true;
}

bool UFacilityPlacementComponent::ValidateFootprintContract(FText& OutFailureReason) const
{
	if (!Definition || !PlacementFootprint)
	{
		OutFailureReason = LOCTEXT("MissingFootprint", "설비 배치 영역을 확인할 수 없습니다.");
		return false;
	}
	return ValidateFootprintContractForDefinition(*Definition, OutFailureReason);
}

bool UFacilityPlacementComponent::ValidateNavigationContract(FText& OutFailureReason) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !PlacementFootprint || !PackagePhysicalRoot)
	{
		OutFailureReason = LOCTEXT("MissingNavigationHelpers", "설비의 navigation helper 구성을 확인할 수 없습니다.");
		return false;
	}
	TInlineComponentArray<UPrimitiveComponent*> Primitives(Owner);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		if (!Primitive)
		{
			continue;
		}
		const FString Name = Primitive->GetName().ToLower();
		const bool bKnownHelper = Primitive == PlacementFootprint || Primitive == PackagePhysicalRoot
			|| !Primitive->IsA<UStaticMeshComponent>() || Primitive->IsA<UInstancedStaticMeshComponent>()
			|| Name.Contains(TEXT("interaction"))
			|| Name.Contains(TEXT("helper")) || Name.Contains(TEXT("slot"))
			|| Name.Contains(TEXT("pile")) || Name.Contains(TEXT("water"))
			|| Name.Contains(TEXT("contents"));
		if ((bKnownHelper && Primitive->CanEverAffectNavigation())
			|| ((Primitive == PlacementFootprint || Primitive == PackagePhysicalRoot)
				&& Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			|| (Primitive->CanEverAffectNavigation()
				&& Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision))
		{
			OutFailureReason = FText::Format(
				LOCTEXT("InvalidNavigationHelper", "설비 helper '{0}'는 collision과 navigation export를 끄고 body Static Mesh만 navigation에 관여해야 합니다."),
				FText::FromName(Primitive->GetFName()));
			return false;
		}
	}
	return true;
}

bool UFacilityPlacementComponent::BuildPlacedActorTransform(
	const FTransform& RequestedTransform,
	FTransform& OutTransform,
	FText& OutFailureReason) const
{
	const AActor* Owner = GetOwner();
	const USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root)
	{
		OutFailureReason = LOCTEXT("MissingPlacedRoot", "배치 설비 클래스의 루트 컴포넌트를 확인할 수 없습니다.");
		return false;
	}
	const FVector RootScale = Root->GetRelativeScale3D();
	if (RootScale.ContainsNaN() || RootScale.GetAbsMin() <= UE_KINDA_SMALL_NUMBER)
	{
		OutFailureReason = LOCTEXT("InvalidPlacedScale", "배치 설비 클래스의 기본 루트 스케일이 올바르지 않습니다.");
		return false;
	}
	OutTransform = RequestedTransform;
	OutTransform.SetScale3D(RootScale);
	FTransform RelativeFootprint;
	if (!GetFootprintRelativeToRoot(RelativeFootprint, OutFailureReason))
	{
		return false;
	}
	const FVector Extent = PlacementFootprint->GetUnscaledBoxExtent();
	const FTransform FootprintAtRequested = RelativeFootprint * OutTransform;
	const FVector BottomCenter = FootprintAtRequested.TransformPosition(
		FVector(0.0f, 0.0f, -Extent.Z));
	OutTransform.SetLocation(
		RequestedTransform.GetLocation() - (BottomCenter - OutTransform.GetLocation()));
	if (OutTransform.ContainsNaN() || !OutTransform.GetRotation().IsNormalized())
	{
		OutFailureReason = LOCTEXT("InvalidPlacedTransform", "배치 설비의 최종 transform이 올바르지 않습니다.");
		return false;
	}
	const FTransform FinalFootprint = RelativeFootprint * OutTransform;
	const FVector FloorNormal = RequestedTransform.GetUnitAxis(EAxis::Z);
	for (const FVector2D Corner : {
		FVector2D(-1.0f, -1.0f), FVector2D(-1.0f, 1.0f),
		FVector2D(1.0f, -1.0f), FVector2D(1.0f, 1.0f) })
	{
		const FVector BottomCorner = FinalFootprint.TransformPosition(
			FVector(Corner.X * Extent.X, Corner.Y * Extent.Y, -Extent.Z));
		if (!FMath::IsNearlyZero(
			FVector::DotProduct(BottomCorner - RequestedTransform.GetLocation(), FloorNormal),
			0.1f))
		{
			OutFailureReason = LOCTEXT("FootprintBottomNotPlanar", "PlacementFootprint의 네 바닥 모서리가 설비 설치 바닥과 일치해야 합니다.");
			return false;
		}
	}
	return true;
}

bool UFacilityPlacementComponent::GetFootprintRelativeToRoot(
	FTransform& OutTransform,
	FText& OutFailureReason) const
{
	const AActor* Owner = GetOwner();
	const USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root || !PlacementFootprint)
	{
		OutFailureReason = LOCTEXT("MissingFootprintRoot", "배치 설비의 루트 또는 배치 영역을 확인할 수 없습니다.");
		return false;
	}

	OutTransform = FTransform::Identity;
	TSet<const USceneComponent*> Visited;
	const USceneComponent* Current = PlacementFootprint;
	while (Current && Current != Root)
	{
		if (Visited.Contains(Current))
		{
			OutFailureReason = LOCTEXT(
				"CyclicFootprintHierarchy",
				"PlacementFootprint의 컴포넌트 계층이 순환하고 있습니다.");
			return false;
		}
		Visited.Add(Current);
		OutTransform = OutTransform * Current->GetRelativeTransform();
		Current = Current->GetAttachParent();
	}
	if (Current != Root)
	{
		OutFailureReason = LOCTEXT(
			"DetachedFootprintHierarchy",
			"PlacementFootprint가 설비 Actor 루트에 연결되어 있지 않습니다.");
		return false;
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
