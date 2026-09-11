#include "Placement/FacilityPlacementComponent.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
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
	UPrimitiveComponent* InPackagePhysicalRoot)
{
	PlacementFootprint = InPlacementFootprint;
	PackagePhysicalRoot = InPackagePhysicalRoot;
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
	ActorCollisionSnapshotState = EActorCollisionSnapshotState::None;
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

bool UFacilityPlacementComponent::PrepareForStagedPlacement(
	UFacilityPlacementDefinition& InDefinition,
	FText& OutFailureReason)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || ActorCollisionSnapshotState != EActorCollisionSnapshotState::None)
	{
		OutFailureReason = LOCTEXT("StagedCollisionSnapshotUnavailable", "설비 배치 collision snapshot을 시작할 수 없습니다.");
		return false;
	}
	bActorCollisionSnapshot = Owner->GetActorEnableCollision();
	ActorCollisionSnapshotState = EActorCollisionSnapshotState::PendingConstruction;
	Owner->SetActorEnableCollision(false);
	Definition = &InDefinition;
	Mode = EPlaceableFacilityMode::Placed;
	bStagedPlacement = true;
	SetPlacedDomainActive(false);
	return true;
}

bool UFacilityPlacementComponent::FinalizeStagedPlacementCollisionSnapshot(FText& OutFailureReason)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !bStagedPlacement
		|| ActorCollisionSnapshotState != EActorCollisionSnapshotState::PendingConstruction)
	{
		OutFailureReason = LOCTEXT("StagedCollisionSnapshotNotPending", "설비 배치 collision snapshot을 확정할 수 없습니다.");
		return false;
	}
	// A deferred actor is forced collision-off before Construction. Preserve the
	// pre-Construction authored value unless Construction explicitly re-enables it.
	bActorCollisionSnapshot = bActorCollisionSnapshot || Owner->GetActorEnableCollision();
	ActorCollisionSnapshotState = EActorCollisionSnapshotState::Captured;
	Owner->SetActorEnableCollision(false);
	return true;
}

bool UFacilityPlacementComponent::ValidateStagedPlacementCollisionSnapshot(
	FText& OutFailureReason) const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !bStagedPlacement
		|| ActorCollisionSnapshotState != EActorCollisionSnapshotState::Captured
		|| Owner->GetActorEnableCollision())
	{
		OutFailureReason = LOCTEXT("InvalidStagedCollisionSnapshot", "설비 배치 collision snapshot 상태가 올바르지 않습니다.");
		return false;
	}
	return true;
}

void UFacilityPlacementComponent::SetPlacedDomainActive(const bool bActive)
{
	bPlacedDomainActive = bActive;
	if (PackagePhysicalRoot)
	{
		PackagePhysicalRoot->SetSimulatePhysics(false);
		PackagePhysicalRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

bool UFacilityPlacementComponent::CommitStagedPlacement(FText& OutFailureReason)
{
	if (!ValidateStagedPlacementCollisionSnapshot(OutFailureReason)
		|| !RestoreActorCollisionSnapshot(OutFailureReason))
	{
		return false;
	}
	bStagedPlacement = false;
	Mode = EPlaceableFacilityMode::Placed;
	SetPlacedDomainActive(true);
	CaptureLastSafeTransform();
	return true;
}

bool UFacilityPlacementComponent::CaptureAndDisableActorCollision(FText& OutFailureReason)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || ActorCollisionSnapshotState != EActorCollisionSnapshotState::None)
	{
		OutFailureReason = LOCTEXT("CollisionSnapshotUnavailable", "설비 Actor collision 상태를 캡처할 수 없습니다.");
		return false;
	}
	bActorCollisionSnapshot = Owner->GetActorEnableCollision();
	ActorCollisionSnapshotState = EActorCollisionSnapshotState::Captured;
	Owner->SetActorEnableCollision(false);
	return true;
}

bool UFacilityPlacementComponent::RestoreActorCollisionSnapshot(FText& OutFailureReason)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || ActorCollisionSnapshotState != EActorCollisionSnapshotState::Captured)
	{
		OutFailureReason = LOCTEXT("MissingCollisionSnapshot", "복원할 설비 Actor collision 상태가 없습니다.");
		return false;
	}
	Owner->SetActorEnableCollision(bActorCollisionSnapshot);
	ActorCollisionSnapshotState = EActorCollisionSnapshotState::None;
	return true;
}

void UFacilityPlacementComponent::ConsumeActorCollisionSnapshot()
{
	ActorCollisionSnapshotState = EActorCollisionSnapshotState::None;
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
