#include "Placement/FacilityPlacementComponent.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#include "NavModifierComponent.h"
#include "Placement/FacilityPlacementCollisionUtils.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "Placement/FacilityPlacementSettings.h"

#define LOCTEXT_NAMESPACE "FacilityPlacementComponent"

UFacilityPlacementComponent::UFacilityPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFacilityPlacementComponent::Configure(
	UBoxComponent* InPlacementFootprint,
	UPrimitiveComponent* InPackagePhysicalRoot,
	UNavModifierComponent* InNavModifier)
{
	PlacementFootprint = InPlacementFootprint;
	PackagePhysicalRoot = InPackagePhysicalRoot;
	NavModifier = InNavModifier;
}

void UFacilityPlacementComponent::BeginPlay()
{
	Super::BeginPlay();
	CaptureLastSafeTransform();
	if (bStagedPlacement)
	{
		SetPlacedDomainActive(false);
		return;
	}
	if (Mode == EPlaceableFacilityMode::Placed)
	{
		FText Ignored;
		ApplyMode(Mode, false, Ignored);
		bPlacedDomainActive = true;
	}
	else
	{
		// Legacy packaged placed-actor assets remain serialized-compatible but inert.
		SetPlacedDomainActive(false);
	}
}

void UFacilityPlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnModeChanged.Clear();
	AssignedFixedSlot.Reset();
	PlacementFootprint = nullptr;
	PackagePhysicalRoot = nullptr;
	NavModifier = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool UFacilityPlacementComponent::IsOperational(FText& OutFailureReason) const
{
	if (!Definition)
	{
		OutFailureReason = LOCTEXT("MissingDefinition", "설비 배치 정의가 없습니다.");
		return false;
	}
	if (!IsValid(PlacementFootprint) || !IsValid(PackagePhysicalRoot)
		|| PackagePhysicalRoot != GetOwner()->GetRootComponent())
	{
		OutFailureReason = LOCTEXT("InvalidComponents", "설비의 배치 영역 또는 포장 물리 루트가 올바르지 않습니다.");
		return false;
	}
	if (bFixedSlotBindingConflict)
	{
		OutFailureReason = LOCTEXT("FixedSlotConflict", "설비의 고정 슬롯 연결이 중복되었습니다.");
		return false;
	}
	return ValidateFootprintContract(OutFailureReason);
}

bool UFacilityPlacementComponent::ValidateFootprintContractForDefinition(
	const UFacilityPlacementDefinition& InDefinition,
	FText& OutFailureReason) const
{
	if (!PlacementFootprint)
	{
		OutFailureReason = LOCTEXT("MissingFootprint", "설비 배치 영역을 확인할 수 없습니다.");
		return false;
	}
	const float Grid = GetDefault<UFacilityPlacementSettings>()->GetGridSizeCm();
	const FVector FullSize = PlacementFootprint->GetScaledBoxExtent() * 2.0f;
	const FVector Expected(InDefinition.FootprintCellsX * Grid, InDefinition.FootprintCellsY * Grid, FullSize.Z);
	if (InDefinition.FootprintCellsX < 1 || InDefinition.FootprintCellsY < 1
		|| !FMath::IsNearlyEqual(FullSize.X, Expected.X, 0.5f)
		|| !FMath::IsNearlyEqual(FullSize.Y, Expected.Y, 0.5f))
	{
		OutFailureReason = LOCTEXT("FootprintGridMismatch", "설비 배치 영역이 정의의 전역 그리드 셀 크기와 일치하지 않습니다.");
		return false;
	}
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
	OutTransform = PlacementFootprint->GetComponentTransform().GetRelativeTransform(
		Root->GetComponentTransform());
	return true;
}

bool UFacilityPlacementComponent::CanEnablePackagedCollision(FText& OutFailureReason) const
{
	if (!PackagePhysicalRoot || !GetWorld() || !GetOwner())
	{
		OutFailureReason = LOCTEXT("MissingPackageCollision", "포장 충돌 상태를 확인할 수 없습니다.");
		return false;
	}
	FTransform RecoveryDropTransform;
	if (!GetRecoveryDropTransform(RecoveryDropTransform, OutFailureReason))
	{
		return false;
	}
	PackagePhysicalRoot->UpdateBounds();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FacilityPackageOverlap), false, GetOwner());
	if (FacilityPlacementCollision::HasBlockingOverlap(
		*GetWorld(),
		RecoveryDropTransform.GetLocation(),
		RecoveryDropTransform.GetRotation(),
		PackagePhysicalRoot->GetCollisionShape(),
		*PackagePhysicalRoot,
		Params))
	{
		OutFailureReason = LOCTEXT("PackageCollisionBlocked", "포장 설비가 다른 물체와 겹쳐 회수할 수 없습니다.");
		return false;
	}
	return true;
}

bool UFacilityPlacementComponent::GetRecoveryDropTransform(
	FTransform& OutTransform,
	FText& OutFailureReason) const
{
	if (!IsValid(PlacementFootprint) || !IsValid(PackagePhysicalRoot) || !IsValid(GetOwner()))
	{
		OutFailureReason = LOCTEXT("MissingRecoveryDropComponents", "설비 회수 위치를 계산할 수 없습니다.");
		return false;
	}

	PlacementFootprint->UpdateBounds();
	FVector DropLocation = PlacementFootprint->GetComponentLocation();
	DropLocation.Z = PlacementFootprint->Bounds.Origin.Z
		- PlacementFootprint->Bounds.BoxExtent.Z
		+ GetDefault<UFacilityPlacementSettings>()->GetRecoveryDropZOffsetCm();

	OutTransform = GetOwner()->GetActorTransform();
	OutTransform.SetLocation(DropLocation);
	OutTransform.SetScale3D(FVector::OneVector);
	return true;
}

bool UFacilityPlacementComponent::BeginTransition(FText& OutFailureReason)
{
	if (bTransitionInProgress)
	{
		OutFailureReason = LOCTEXT("TransitionBusy", "설비 상태를 이미 변경하는 중입니다.");
		return false;
	}
	bTransitionInProgress = true;
	return true;
}

void UFacilityPlacementComponent::EndTransition()
{
	bTransitionInProgress = false;
}

bool UFacilityPlacementComponent::ApplyMode(
	const EPlaceableFacilityMode NewMode,
	const bool bFreeWorldPhysics,
	FText& OutFailureReason,
	const bool bPublish)
{
	if (NewMode == EPlaceableFacilityMode::Packaged)
	{
		OutFailureReason = LOCTEXT("LegacyPackagedModeDisabled", "배치 설비 Actor는 더 이상 포장 상태로 전환되지 않습니다.");
		return false;
	}
	if (!IsValid(PackagePhysicalRoot))
	{
		OutFailureReason = LOCTEXT("MissingPackageRoot", "포장 물리 루트가 없습니다.");
		return false;
	}
	const EPlaceableFacilityMode Previous = Mode;
	Mode = NewMode;
	if (NavModifier)
	{
		NavModifier->SetNavigationRelevancy(NewMode == EPlaceableFacilityMode::Placed);
	}
	bPlacedDomainActive = NewMode == EPlaceableFacilityMode::Placed;
	if (NewMode == EPlaceableFacilityMode::Placed)
	{
		PackagePhysicalRoot->SetSimulatePhysics(false);
		PackagePhysicalRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CaptureLastSafeTransform();
	}
	else if (bFreeWorldPhysics)
	{
		PackagePhysicalRoot->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		PackagePhysicalRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		PackagePhysicalRoot->BodyInstance.bUseCCD = true;
		PackagePhysicalRoot->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PackagePhysicalRoot->SetSimulatePhysics(true);
	}
	else
	{
		PackagePhysicalRoot->SetSimulatePhysics(false);
		PackagePhysicalRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (bPublish && Previous != Mode)
	{
		OnModeChanged.Broadcast(Previous, Mode);
	}
	return true;
}

void UFacilityPlacementComponent::PrepareForStagedPlacement(
	UFacilityPlacementDefinition& InDefinition)
{
	Definition = &InDefinition;
	Mode = EPlaceableFacilityMode::Placed;
	bStagedPlacement = true;
	SetPlacedDomainActive(false);
}

void UFacilityPlacementComponent::SetPlacedDomainActive(const bool bActive)
{
	bPlacedDomainActive = bActive;
	if (NavModifier)
	{
		NavModifier->SetNavigationRelevancy(bActive);
	}
	if (PackagePhysicalRoot)
	{
		PackagePhysicalRoot->SetSimulatePhysics(false);
		PackagePhysicalRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void UFacilityPlacementComponent::CommitStagedPlacement()
{
	bStagedPlacement = false;
	Mode = EPlaceableFacilityMode::Placed;
	SetPlacedDomainActive(true);
	CaptureLastSafeTransform();
}

void UFacilityPlacementComponent::PublishModeChanged(
	const EPlaceableFacilityMode PreviousMode,
	const EPlaceableFacilityMode NewMode)
{
	if (PreviousMode != NewMode && Mode == NewMode)
	{
		OnModeChanged.Broadcast(PreviousMode, NewMode);
	}
}

void UFacilityPlacementComponent::ApplyHeldPresentation(
	USceneComponent& HeldAnchor,
	const FTransform& InHeldTransform)
{
	if (!PackagePhysicalRoot)
	{
		return;
	}
	PackagePhysicalRoot->SetSimulatePhysics(false);
	PackagePhysicalRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PackagePhysicalRoot->AttachToComponent(&HeldAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	PackagePhysicalRoot->SetRelativeTransform(InHeldTransform);
}

void UFacilityPlacementComponent::CaptureLastSafeTransform()
{
	if (AActor* Owner = GetOwner())
	{
		LastSafeTransform = Owner->GetActorTransform();
	}
}

void UFacilityPlacementComponent::RestoreLastSafePackagedWorld()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorTransform(LastSafeTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}
	FText Ignored;
	ApplyMode(EPlaceableFacilityMode::Packaged, true, Ignored);
}

bool UFacilityPlacementComponent::TryBindFixedSlot(AActor& SlotActor, FText& OutFailureReason)
{
	if (AssignedFixedSlot.IsValid() && AssignedFixedSlot.Get() != &SlotActor)
	{
		OutFailureReason = LOCTEXT("AlreadyBound", "설비가 이미 다른 고정 슬롯에 연결되어 있습니다.");
		return false;
	}
	AssignedFixedSlot = &SlotActor;
	return true;
}

void UFacilityPlacementComponent::ClearFixedSlot(AActor& ExpectedSlot)
{
	if (AssignedFixedSlot.Get() == &ExpectedSlot)
	{
		AssignedFixedSlot.Reset();
	}
}

bool UFacilityPlacementComponent::IsStoredInFixedSlot() const
{
	const IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(AssignedFixedSlot.Get());
	return Slot && Slot->GetStoredPhysicalCarryItem() == GetOwner();
}

#undef LOCTEXT_NAMESPACE
