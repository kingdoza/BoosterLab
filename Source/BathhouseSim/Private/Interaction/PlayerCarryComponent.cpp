#include "Interaction/PlayerCarryComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#include "Interaction/PhysicalCarryPlacementTransaction.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerEquipmentUseComponent.h"

#define LOCTEXT_NAMESPACE "PlayerCarryComponent"

UPlayerCarryComponent::UPlayerCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AActor* PreviousHeldObject = HeldObject.Get();
	if (IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(PreviousHeldObject))
	{
		if (!RecoverHeldPhysicalObject(PreviousHeldObject))
		{
			CancelEquipmentUseForPlacement();
			bool bWasKey = false;
			if (ClearHeldObjectWithoutNotification(PreviousHeldObject, bWasKey))
			{
				PublishHeldObjectChange(nullptr, bWasKey);
			}
			Carryable->RecoverPhysicalCarryable(this);
		}
	}
	else
	{
		CancelEquipmentUseForPlacement();
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
	if (bPhysicalDropCommitInProgress)
	{
		return false;
	}
	TGuardValue<bool> PlacementGuard(bPhysicalDropCommitInProgress, true);
	if (!CommitHeldObjectWithoutNotification(Key))
	{
		return false;
	}
	PublishHeldObjectChange(Key, false);
	return HeldObject == Key;
}

bool UPlayerCarryComponent::CommitReleaseKey(ABathhouseKeyActor* Key)
{
	return CommitReleasePhysicalObject(Key);
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
	if (bPhysicalDropCommitInProgress)
	{
		OutFailureReason = LOCTEXT("PlacementAlreadyInProgress", "이미 물건 위치를 변경하는 중입니다.");
		return false;
	}
	if (!IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	if (!IsValid(HeldAnchor) || !Carryable->CanBeTakenBy(*this, OutFailureReason))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = LOCTEXT("TakeUnavailable", "물건을 들 수 없습니다.");
		}
		return false;
	}

	UPrimitiveComponent* Primitive = Carryable->GetPhysicalCarryPrimitive();
	if (!IsValid(Primitive) || Primitive != Object->GetRootComponent())
	{
		OutFailureReason = LOCTEXT("InvalidCarryPrimitive", "물건의 물리 루트 설정이 올바르지 않습니다.");
		return false;
	}

	FPhysicalCarryPlacementTransaction Transaction(*Object, *Primitive);
	if (!Transaction.IsValid())
	{
		OutFailureReason = LOCTEXT("TakeFailed", "물건을 들 수 없습니다.");
		return false;
	}

	TGuardValue<bool> PlacementGuard(bPhysicalDropCommitInProgress, true);
	if (!CommitHeldObjectWithoutNotification(Object))
	{
		OutFailureReason = LOCTEXT("TakeCommitFailed", "소지 상태를 적용할 수 없습니다.");
		return false;
	}
	if (!Carryable->HandleTakenBy(*this, HeldAnchor))
	{
		bool bWasKey = false;
		ClearHeldObjectWithoutNotification(Object, bWasKey);
		OutFailureReason = LOCTEXT("TakeFailed", "물건을 들 수 없습니다.");
		return false;
	}
	Transaction.Commit();
	PublishPhysicalCarryCommit(
		Object,
		nullptr,
		EPhysicalCarryCommitTransition::TakenIntoHand,
		false,
		false);
	return true;
}

bool UPlayerCarryComponent::CommitReleasePhysicalObject(AActor* Object)
{
	if (bPhysicalDropCommitInProgress)
	{
		return false;
	}
	TGuardValue<bool> PlacementGuard(bPhysicalDropCommitInProgress, true);
	bool bWasKey = false;
	if (!ClearHeldObjectWithoutNotification(Object, bWasKey))
	{
		return false;
	}
	PublishHeldObjectChange(nullptr, bWasKey);
	return true;
}

bool UPlayerCarryComponent::RecoverHeldPhysicalObject(AActor* Object)
{
	IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(Object);
	if (bPhysicalDropCommitInProgress || !IsValid(Object) || !Carryable || HeldObject != Object)
	{
		return false;
	}

	TGuardValue<bool> PlacementGuard(bPhysicalDropCommitInProgress, true);
	CancelEquipmentUseForPlacement();
	if (!IsValid(Object) || HeldObject != Object)
	{
		return HeldObject != Object;
	}
	bool bWasKey = false;
	if (!ClearHeldObjectWithoutNotification(Object, bWasKey))
	{
		return false;
	}

	Carryable->RecoverPhysicalCarryable(this);
	PublishHeldObjectChange(nullptr, bWasKey);
	if (IsValid(Object))
	{
		if (IPhysicalCarryable* RecoveredCarryable = Cast<IPhysicalCarryable>(Object))
		{
			RecoveredCarryable->PublishPhysicalCarryCommit(EPhysicalCarryCommitTransition::Recovered);
		}
	}
	return true;
}

FPlayerInteractionResult UPlayerCarryComponent::TryTakeFromFixedSlot(AActor* SlotActor)
{
	if (bPhysicalDropCommitInProgress)
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("PlacementAlreadyInProgress", "이미 물건 위치를 변경하는 중입니다."));
	}
	IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(SlotActor);
	FText FailureReason;
	if (!IsValid(SlotActor) || !Slot || !Slot->QueryTakePhysicalCarry(*this, FailureReason))
	{
		return FPlayerInteractionResult::Failed(
			FailureReason.IsEmpty()
				? LOCTEXT("InvalidFixedSlot", "고정 슬롯 상태를 확인할 수 없습니다.")
				: FailureReason);
	}

	AActor* Object = Slot->GetStoredPhysicalCarryItem();
	IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(Object);
	UPrimitiveComponent* Primitive = Carryable ? Carryable->GetPhysicalCarryPrimitive() : nullptr;
	USceneComponent* Anchor = HeldAnchor.Get();
	if (!IsValid(Object) || !Carryable || !IsValid(Primitive) || Primitive != Object->GetRootComponent()
		|| !IsValid(Anchor) || Carryable->GetAssignedPhysicalCarryFixedSlot() != SlotActor)
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("InvalidFixedTakeState", "슬롯과 물건 연결 상태가 올바르지 않습니다."));
	}

	TGuardValue<bool> PlacementGuard(bPhysicalDropCommitInProgress, true);
	CancelEquipmentUseForPlacement();
	if (!IsValid(Object) || !IsValid(SlotActor) || !IsValid(Primitive) || !IsValid(Anchor)
		|| !IsHandEmpty() || Slot->GetStoredPhysicalCarryItem() != Object)
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("FixedTakeInvalidated", "슬롯에서 물건을 가져오는 중 상태가 변경되었습니다."));
	}
	FPhysicalCarryPlacementTransaction Transaction(*Object, *Primitive, SlotActor);
	if (!Transaction.IsValid()
		|| !Transaction.ApplyHeld(*Anchor, Carryable->GetHeldTransform())
		|| !Transaction.ApplySlotOccupancy(false))
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("FixedTakeFailed", "슬롯에서 물건을 가져올 수 없습니다."));
	}
	if (!CommitHeldObjectWithoutNotification(Object))
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("FixedTakeCommitFailed", "소지 상태를 적용할 수 없습니다."));
	}
	if (!Carryable->NotifyTakenFromFixedSlotCommitted(*this, *SlotActor))
	{
		bool bWasKey = false;
		ClearHeldObjectWithoutNotification(Object, bWasKey);
		return FPlayerInteractionResult::Failed(
			LOCTEXT("FixedTakeFailed", "슬롯에서 물건을 가져올 수 없습니다."));
	}
	Transaction.Commit();
	PublishPhysicalCarryCommit(
		Object,
		SlotActor,
		EPhysicalCarryCommitTransition::TakenIntoHand,
		true,
		false);
	return FPlayerInteractionResult::Succeeded();
}

FPlayerInteractionResult UPlayerCarryComponent::TryStoreHeldObjectInFixedSlot(AActor* SlotActor)
{
	if (bPhysicalDropCommitInProgress)
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("PlacementAlreadyInProgress", "이미 물건 위치를 변경하는 중입니다."));
	}
	IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(SlotActor);
	AActor* Object = HeldObject.Get();
	FText FailureReason;
	if (!IsValid(SlotActor) || !Slot || !IsValid(Object)
		|| !Slot->QueryStorePhysicalCarry(*this, *Object, FailureReason))
	{
		return FPlayerInteractionResult::Failed(
			FailureReason.IsEmpty()
				? LOCTEXT("InvalidFixedStore", "고정 슬롯에 물건을 놓을 수 없습니다.")
				: FailureReason);
	}

	IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(Object);
	UPrimitiveComponent* Primitive = Carryable ? Carryable->GetPhysicalCarryPrimitive() : nullptr;
	USceneComponent* Anchor = Slot->GetPhysicalCarryItemAnchor();
	if (!Carryable || !IsValid(Primitive) || Primitive != Object->GetRootComponent()
		|| !IsValid(Anchor) || Slot->GetAssignedPhysicalCarryItem() != Object
		|| Carryable->GetAssignedPhysicalCarryFixedSlot() != SlotActor)
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("InvalidFixedStoreState", "슬롯과 물건 연결 상태가 올바르지 않습니다."));
	}

	TGuardValue<bool> PlacementGuard(bPhysicalDropCommitInProgress, true);
	CancelEquipmentUseForPlacement();
	if (!IsValid(Object) || !IsValid(SlotActor) || !IsValid(Primitive) || !IsValid(Anchor)
		|| HeldObject != Object || Slot->GetAssignedPhysicalCarryItem() != Object)
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("FixedStoreInvalidated", "슬롯에 물건을 놓는 중 상태가 변경되었습니다."));
	}
	FPhysicalCarryPlacementTransaction Transaction(*Object, *Primitive, SlotActor);
	if (!Transaction.IsValid()
		|| !Transaction.ApplyFixedSlot(*Anchor)
		|| !Transaction.ApplySlotOccupancy(true))
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("FixedStoreFailed", "슬롯에 물건을 놓을 수 없습니다."));
	}
	bool bWasKey = false;
	if (!ClearHeldObjectWithoutNotification(Object, bWasKey))
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("FixedStoreCommitFailed", "소지 상태를 해제할 수 없습니다."));
	}
	if (!Carryable->NotifyStoredInFixedSlotCommitted(*this, *SlotActor))
	{
		ensureMsgf(
			CommitHeldObjectWithoutNotification(Object),
			TEXT("Failed to restore HeldObject after fixed-slot domain preparation rejected the transition."));
		return FPlayerInteractionResult::Failed(
			LOCTEXT("FixedStoreFailed", "슬롯에 물건을 놓을 수 없습니다."));
	}
	Transaction.Commit();
	PublishPhysicalCarryCommit(
		Object,
		SlotActor,
		EPhysicalCarryCommitTransition::StoredInFixedSlot,
		true,
		bWasKey);
	return FPlayerInteractionResult::Succeeded();
}

FPlayerInteractionResult UPlayerCarryComponent::TryFreeDropHeldObject(const FVector& Direction)
{
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
		return FPlayerInteractionResult::Failed(
			FailureReason.IsEmpty()
				? LOCTEXT("CannotDrop", "이 물건은 여기에 내려놓을 수 없습니다.")
				: FailureReason,
			EPlayerInteractionIntent::DropCarry);
	}

	UPrimitiveComponent* Primitive = Carryable->GetPhysicalCarryPrimitive();
	if (!IsValid(Primitive) || Primitive != Object->GetRootComponent())
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("InvalidDropPrimitive", "물건의 드랍 충돌 설정이 올바르지 않습니다."),
			EPlayerInteractionIntent::DropCarry);
	}

	const FVector Forward = Direction.GetSafeNormal();
	if (Forward.IsNearlyZero() || Forward.ContainsNaN())
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("InvalidDropDirection", "물건을 내려놓을 방향이 올바르지 않습니다."),
			EPlayerInteractionIntent::DropCarry);
	}
	TGuardValue<bool> PlacementGuard(bPhysicalDropCommitInProgress, true);
	CancelEquipmentUseForPlacement();
	if (!IsValid(Object) || HeldObject != Object || !IsValid(Primitive))
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("DropInvalidated", "물건을 내려놓는 중 상태가 변경되었습니다."),
			EPlayerInteractionIntent::DropCarry);
	}
	if (!IsHeldPoseClear(*Object, *Primitive, FailureReason))
	{
		return FPlayerInteractionResult::Failed(FailureReason, EPlayerInteractionIntent::DropCarry);
	}

	const float ForwardVelocity = FMath::Max(0.0f, Carryable->GetThrowImpulseStrength());
	const float UpwardVelocity = FMath::Max(0.0f, Carryable->GetUpwardThrowImpulseStrength());
	const FVector VelocityChange = Forward * ForwardVelocity + FVector::UpVector * UpwardVelocity;
	FPhysicalCarryPlacementTransaction Transaction(*Object, *Primitive);
	if (!Transaction.IsValid() || !Transaction.ApplyFreeWorld(VelocityChange))
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("DropPhysicsFailed", "물건의 물리를 활성화할 수 없습니다."),
			EPlayerInteractionIntent::DropCarry);
	}

	bool bWasKey = false;
	if (!ClearHeldObjectWithoutNotification(Object, bWasKey))
	{
		return FPlayerInteractionResult::Failed(
			LOCTEXT("DropCommitFailed", "소지 상태를 해제할 수 없습니다."),
			EPlayerInteractionIntent::DropCarry);
	}
	if (!Carryable->NotifyPhysicalDropCommitted(*this))
	{
		ensureMsgf(
			CommitHeldObjectWithoutNotification(Object),
			TEXT("Failed to restore HeldObject after free-drop domain preparation rejected the transition."));
		return FPlayerInteractionResult::Failed(
			LOCTEXT("DropCommitFailed", "소지 상태를 해제할 수 없습니다."),
			EPlayerInteractionIntent::DropCarry);
	}
	Transaction.Commit();
	PublishPhysicalCarryCommit(
		Object,
		nullptr,
		EPhysicalCarryCommitTransition::DroppedToWorld,
		false,
		bWasKey);
	return FPlayerInteractionResult::Succeeded(EPlayerInteractionIntent::DropCarry);
}

FPlayerInteractionResult UPlayerCarryComponent::TryReleaseHeldEquipment(
	const FVector& ViewOrigin,
	const FVector& ThrowDirection)
{
	return TryFreeDropHeldObject(ThrowDirection);
}

bool UPlayerCarryComponent::IsHeldPoseClear(
	AActor& Object,
	UPrimitiveComponent& Primitive,
	FText& OutFailureReason) const
{
	UWorld* World = GetWorld();
	Primitive.UpdateBounds();
	const FVector BoxExtent = Primitive.Bounds.BoxExtent;
	if (!World || BoxExtent.ContainsNaN() || BoxExtent.GetMin() <= KINDA_SMALL_NUMBER)
	{
		OutFailureReason = LOCTEXT("InvalidDropBounds", "물건의 드랍 충돌 크기가 올바르지 않습니다.");
		return false;
	}

	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhysicalCarryHeldPoseOverlap), false);
	QueryParams.AddIgnoredActor(&Object);
	if (AActor* CarryOwner = GetOwner())
	{
		QueryParams.AddIgnoredActor(CarryOwner);
	}
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Primitive.GetComponentLocation(),
		Primitive.GetComponentQuat(),
		ObjectTypes,
		Primitive.GetCollisionShape(),
		QueryParams);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const UPrimitiveComponent* Other = Overlap.Component.Get();
		if (!IsValid(Other) || Other == &Primitive
			|| Other->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}
		const bool bMutuallyBlocking =
			Primitive.GetCollisionResponseToChannel(Other->GetCollisionObjectType()) == ECR_Block
			&& Other->GetCollisionResponseToChannel(Primitive.GetCollisionObjectType()) == ECR_Block;
		if (bMutuallyBlocking)
		{
			OutFailureReason = LOCTEXT(
				"HeldPoseBlocked",
				"손에 든 위치가 막혀 있어 물건을 내려놓을 수 없습니다.");
			return false;
		}
	}
	return true;
}

void UPlayerCarryComponent::CancelEquipmentUseForPlacement()
{
	if (EquipmentUseComponent)
	{
		EquipmentUseComponent->CancelEquipmentUse();
	}
}

void UPlayerCarryComponent::NotifyHeldActorEnding(AActor* Object)
{
	if (HeldObject != Object)
	{
		return;
	}
	CancelEquipmentUseForPlacement();
	bool bWasKey = false;
	if (ClearHeldObjectWithoutNotification(Object, bWasKey))
	{
		PublishHeldObjectChange(nullptr, bWasKey);
	}
}

bool UPlayerCarryComponent::CommitHeldObjectWithoutNotification(AActor* Object)
{
	if (!IsValid(Object) || !IsHandEmpty())
	{
		return false;
	}
	HeldObject = Object;
	return true;
}

bool UPlayerCarryComponent::ClearHeldObjectWithoutNotification(
	AActor* ExpectedObject,
	bool& bOutWasKey)
{
	if (!ExpectedObject || HeldObject != ExpectedObject)
	{
		return false;
	}
	bOutWasKey = Cast<ABathhouseKeyActor>(HeldObject.Get()) != nullptr;
	HeldObject = nullptr;
	return true;
}

void UPlayerCarryComponent::PublishHeldObjectChange(
	AActor* NewHeldObject,
	const bool bPreviousObjectWasKey)
{
	OnHeldObjectChanged.Broadcast(NewHeldObject);
	if (NewHeldObject)
	{
		if (HeldObject == NewHeldObject && IsValid(NewHeldObject))
		{
			OnHeldKeyChanged.Broadcast(Cast<ABathhouseKeyActor>(NewHeldObject));
		}
	}
	else if (bPreviousObjectWasKey)
	{
		OnHeldKeyChanged.Broadcast(nullptr);
	}
}

void UPlayerCarryComponent::PublishPhysicalCarryCommit(
	AActor* Object,
	AActor* SlotActor,
	const EPhysicalCarryCommitTransition Transition,
	const bool bNotifySlot,
	const bool bPreviousObjectWasKey)
{
	const bool bTakingIntoHand = Transition == EPhysicalCarryCommitTransition::TakenIntoHand;
	PublishHeldObjectChange(bTakingIntoHand ? Object : nullptr, bPreviousObjectWasKey);
	if (!IsValid(Object) || (bTakingIntoHand && HeldObject != Object))
	{
		return;
	}

	if (bNotifySlot && IsValid(SlotActor))
	{
		if (IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(SlotActor))
		{
			Slot->NotifyPhysicalCarrySlotOccupancyCommitted();
		}
	}
	if (!IsValid(Object))
	{
		return;
	}
	if (IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(Object))
	{
		Carryable->PublishPhysicalCarryCommit(Transition);
	}
}

#undef LOCTEXT_NAMESPACE
