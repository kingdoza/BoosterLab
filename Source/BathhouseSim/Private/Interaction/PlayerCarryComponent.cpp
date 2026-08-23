#include "Interaction/PlayerCarryComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerEquipmentUseComponent.h"

#define LOCTEXT_NAMESPACE "PlayerCarryComponent"

UPlayerCarryComponent::UPlayerCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EquipmentUseComponent)
	{
		EquipmentUseComponent->CancelEquipmentUse();
	}
	AActor* PreviousHeldObject = HeldObject.Get();
	if (ABathhouseKeyActor* HeldKey = Cast<ABathhouseKeyActor>(PreviousHeldObject))
	{
		HeldKey->RecoverToHook(this);
	}
	else if (IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(PreviousHeldObject))
	{
		ClearHeldObject(PreviousHeldObject);
		Carryable->RecoverPhysicalCarryable(this);
	}
	HeldObject = nullptr;
	HeldAnchor = nullptr;
	EquipmentUseComponent = nullptr;
	OnHeldKeyChanged.Clear();
	OnHeldObjectChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

void UPlayerCarryComponent::ConfigureEquipmentUse(UPlayerEquipmentUseComponent* InEquipmentUse)
{
	EquipmentUseComponent = InEquipmentUse;
}

void UPlayerCarryComponent::ConfigureHeldAnchor(USceneComponent* InHeldAnchor)
{
	HeldAnchor = InHeldAnchor;
}

bool UPlayerCarryComponent::CommitTakeKey(ABathhouseKeyActor* Key)
{
	return CommitHeldObject(Key);
}

bool UPlayerCarryComponent::CommitReleaseKey(ABathhouseKeyActor* Key)
{
	// Pointer identity is sufficient for release. In particular, a key actor can
	// already be ending play when it asks its carry owner to drop the reference.
	if (!Key || HeldObject != Key)
	{
		return false;
	}
	return ClearHeldObject(Key);
}

ABathhouseKeyActor* UPlayerCarryComponent::GetHeldKey() const
{
	return Cast<ABathhouseKeyActor>(HeldObject.Get());
}

EPhysicalCarryKind UPlayerCarryComponent::GetHeldKind() const
{
	const IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(HeldObject.Get());
	return Carryable ? Carryable->GetPhysicalCarryKind() : EPhysicalCarryKind::None;
}

bool UPlayerCarryComponent::TryTakePhysicalObject(AActor* Object, FText& OutFailureReason)
{
	IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(Object);
	if (!IsValid(Object) || !Carryable)
	{
		OutFailureReason = LOCTEXT("InvalidCarryable", "이 물건은 들 수 없습니다.");
		return false;
	}
	if (!IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	if (!Carryable->CanBeTakenBy(*this, OutFailureReason) || !CommitHeldObject(Object))
	{
		return false;
	}
	if (!Carryable->HandleTakenBy(*this, HeldAnchor))
	{
		ClearHeldObject(Object);
		OutFailureReason = LOCTEXT("TakeFailed", "물건을 들 수 없습니다.");
		return false;
	}
	return true;
}

bool UPlayerCarryComponent::CommitReleasePhysicalObject(AActor* Object)
{
	return ClearHeldObject(Object);
}

FPlayerInteractionResult UPlayerCarryComponent::TryReleaseHeldEquipment(
	const FVector& ViewOrigin,
	const FVector& ThrowDirection)
{
	if (EquipmentUseComponent)
	{
		EquipmentUseComponent->CancelEquipmentUse();
	}
	if (bPhysicalDropCommitInProgress)
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("DropAlreadyInProgress", "이미 물건을 내려놓는 중입니다."),
			EPlayerInteractionIntent::DropCarry);
	}

	AActor* Object = HeldObject.Get();
	IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(Object);
	FText FailureReason;
	if (!Object)
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("NothingHeld", "내려놓을 장비가 없습니다."),
			EPlayerInteractionIntent::DropCarry);
	}
	if (!Carryable || !Carryable->CanFreeDrop(FailureReason))
	{
		if (FailureReason.IsEmpty())
		{
			FailureReason = LOCTEXT("CannotDrop", "이 물건은 여기에 내려놓을 수 없습니다.");
		}
		return FPlayerInteractionResult::Failed(FailureReason, EPlayerInteractionIntent::DropCarry);
	}

	UPrimitiveComponent* Primitive = Carryable->GetPhysicalCarryPrimitive();
	if (!IsValid(Primitive) || Primitive != Object->GetRootComponent())
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("InvalidDropPrimitive", "물건의 드랍 충돌 설정이 올바르지 않습니다."),
			EPlayerInteractionIntent::DropCarry);
	}

	const FVector NormalizedThrowDirection = ThrowDirection.GetSafeNormal();
	if (NormalizedThrowDirection.IsNearlyZero() || NormalizedThrowDirection.ContainsNaN())
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("InvalidDropDirection", "물건을 내려놓을 방향이 올바르지 않습니다."),
			EPlayerInteractionIntent::DropCarry);
	}

	FVector SafeActorLocation;
	if (!ResolvePhysicalDropLocation(
		*Object,
		*Primitive,
		ViewOrigin,
		NormalizedThrowDirection,
		Carryable->GetThrowSpawnDistance(),
		SafeActorLocation,
		FailureReason))
	{
		return FPlayerInteractionResult::Failed(FailureReason, EPlayerInteractionIntent::DropCarry);
	}

	USceneComponent* PreviousAttachParent = Primitive->GetAttachParent();
	const FName PreviousAttachSocket = Primitive->GetAttachSocketName();
	const FTransform PreviousRelativeTransform = Primitive->GetRelativeTransform();
	const FTransform PreviousActorTransform = Object->GetActorTransform();
	const ECollisionEnabled::Type PreviousCollision = Primitive->GetCollisionEnabled();
	TGuardValue<bool> DropCommitGuard(bPhysicalDropCommitInProgress, true);

	auto RestoreHeldPlacement = [&]()
	{
		Primitive->SetSimulatePhysics(false);
		Primitive->SetCollisionEnabled(PreviousCollision);
		Object->SetActorTransform(
			PreviousActorTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		if (PreviousAttachParent)
		{
			Primitive->AttachToComponent(
				PreviousAttachParent,
				FAttachmentTransformRules::KeepWorldTransform,
				PreviousAttachSocket);
			Primitive->SetRelativeTransform(PreviousRelativeTransform);
		}
	};

	Object->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (!Object->SetActorLocation(
		SafeActorLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics))
	{
		RestoreHeldPlacement();
		return FPlayerInteractionResult::Failed(
			LOCTEXT("DropPlacementFailed", "물건을 내려놓을 위치를 적용할 수 없습니다."),
			EPlayerInteractionIntent::DropCarry);
	}

	Primitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Primitive->SetSimulatePhysics(true);
	if (!Primitive->IsSimulatingPhysics())
	{
		RestoreHeldPlacement();
		return FPlayerInteractionResult::Failed(
			LOCTEXT("DropPhysicsFailed", "물건의 물리를 활성화할 수 없습니다."),
			EPlayerInteractionIntent::DropCarry);
	}

	Primitive->AddImpulse(
		NormalizedThrowDirection * Carryable->GetThrowImpulseStrength(),
		NAME_None,
		true);
	Carryable->NotifyPhysicalDropCommitted(*this);
	const bool bClearedHeldObject = ClearHeldObject(Object);
	ensureMsgf(
		bClearedHeldObject || HeldObject == nullptr,
		TEXT("Physical drop committed but the carry component retained an unexpected held object."));
	return FPlayerInteractionResult::Succeeded(EPlayerInteractionIntent::DropCarry);
}

bool UPlayerCarryComponent::ResolvePhysicalDropLocation(
	AActor& Object,
	UPrimitiveComponent& Primitive,
	const FVector& ViewOrigin,
	const FVector& ThrowDirection,
	const float ThrowSpawnDistance,
	FVector& OutSafeActorLocation,
	FText& OutFailureReason) const
{
	OutSafeActorLocation = Object.GetActorLocation();
	if (!GetWorld()
		|| ViewOrigin.ContainsNaN()
		|| ThrowDirection.IsNearlyZero()
		|| ThrowDirection.ContainsNaN()
		|| !FMath::IsFinite(ThrowSpawnDistance)
		|| ThrowSpawnDistance < 0.0f)
	{
		OutFailureReason = LOCTEXT("InvalidDropRequest", "물건을 내려놓을 위치를 계산할 수 없습니다.");
		return false;
	}

	const FVector BoxExtent = Primitive.Bounds.BoxExtent;
	if (BoxExtent.ContainsNaN()
		|| !FMath::IsFinite(BoxExtent.X)
		|| !FMath::IsFinite(BoxExtent.Y)
		|| !FMath::IsFinite(BoxExtent.Z)
		|| BoxExtent.GetMin() <= KINDA_SMALL_NUMBER)
	{
		OutFailureReason = LOCTEXT("InvalidDropBounds", "물건의 드랍 충돌 크기가 올바르지 않습니다.");
		return false;
	}

	const FVector DesiredActorLocation = ViewOrigin + ThrowDirection * ThrowSpawnDistance;
	const FVector BoundsOffset = Primitive.Bounds.Origin - Object.GetActorLocation();
	const FVector SweepStart = Primitive.Bounds.Origin;
	const FVector SweepEnd = DesiredActorLocation + BoundsOffset;
	const FVector SweepDelta = SweepEnd - SweepStart;
	const FCollisionShape SweepShape = FCollisionShape::MakeBox(BoxExtent);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhysicalCarryDropWallSweep), false);
	QueryParams.bFindInitialOverlaps = true;
	QueryParams.AddIgnoredActor(&Object);
	if (AActor* CarryOwner = GetOwner())
	{
		QueryParams.AddIgnoredActor(CarryOwner);
	}

	FHitResult Hit;
	if (!GetWorld()->SweepSingleByChannel(
		Hit,
		SweepStart,
		SweepEnd,
		FQuat::Identity,
		DropSweepChannel,
		SweepShape,
		QueryParams))
	{
		OutSafeActorLocation = DesiredActorLocation;
		return true;
	}

	const float Clearance = FMath::Max(0.0f, DropSweepClearance);
	FVector CandidateSweepCenter;
	if (Hit.bStartPenetrating)
	{
		const FVector DepenetrationNormal = Hit.Normal.GetSafeNormal();
		if (DepenetrationNormal.IsNearlyZero() || DepenetrationNormal.ContainsNaN())
		{
			OutFailureReason = LOCTEXT("NoDropSpace", "앞에 물건을 내려놓을 공간이 없습니다.");
			return false;
		}
		const FVector DepenetrationDelta = DepenetrationNormal
			* (FMath::Max(0.0f, Hit.PenetrationDepth) + Clearance);
		if (FVector::DotProduct(DepenetrationDelta, ThrowDirection) > 0.0)
		{
			// A target-side MTD can move an initially overlapping shape through a thin
			// blocker. The allowed placement segment has no safe player-side point in
			// that direction, so preserve the held transaction instead.
			OutFailureReason = LOCTEXT("NoDropSpace", "앞에 물건을 내려놓을 공간이 없습니다.");
			return false;
		}
		CandidateSweepCenter = SweepStart + DepenetrationDelta;
	}
	else
	{
		CandidateSweepCenter = Hit.Location - ThrowDirection * Clearance;
	}

	const double SweepLengthSquared = SweepDelta.SizeSquared();
	if (SweepLengthSquared <= UE_DOUBLE_SMALL_NUMBER)
	{
		CandidateSweepCenter = SweepStart;
	}
	else
	{
		const double SegmentAlpha = FVector::DotProduct(CandidateSweepCenter - SweepStart, SweepDelta)
			/ SweepLengthSquared;
		CandidateSweepCenter = SweepStart + SweepDelta * FMath::Clamp(SegmentAlpha, 0.0, 1.0);
	}

	if (GetWorld()->OverlapBlockingTestByChannel(
		CandidateSweepCenter,
		FQuat::Identity,
		DropSweepChannel,
		SweepShape,
		QueryParams))
	{
		OutFailureReason = LOCTEXT("NoDropSpace", "앞에 물건을 내려놓을 공간이 없습니다.");
		return false;
	}

	OutSafeActorLocation = CandidateSweepCenter - BoundsOffset;
	if (OutSafeActorLocation.ContainsNaN())
	{
		OutFailureReason = LOCTEXT("InvalidSafeDropLocation", "물건을 내려놓을 위치를 계산할 수 없습니다.");
		return false;
	}
	return true;
}

void UPlayerCarryComponent::NotifyHeldActorEnding(AActor* Object)
{
	if (HeldObject == Object && EquipmentUseComponent)
	{
		EquipmentUseComponent->CancelEquipmentUse();
	}
	ClearHeldObject(Object);
}

bool UPlayerCarryComponent::CommitHeldObject(AActor* Object)
{
	if (!IsValid(Object) || !IsHandEmpty())
	{
		return false;
	}
	HeldObject = Object;
	OnHeldObjectChanged.Broadcast(Object);
	OnHeldKeyChanged.Broadcast(Cast<ABathhouseKeyActor>(Object));
	return true;
}

bool UPlayerCarryComponent::ClearHeldObject(AActor* ExpectedObject)
{
	if (!ExpectedObject || HeldObject != ExpectedObject)
	{
		return false;
	}
	const bool bWasKey = Cast<ABathhouseKeyActor>(HeldObject.Get()) != nullptr;
	HeldObject = nullptr;
	OnHeldObjectChanged.Broadcast(nullptr);
	if (bWasKey)
	{
		OnHeldKeyChanged.Broadcast(nullptr);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
