#include "Combat/MonkeyWrenchActor.h"

#include "Combat/MeleeAttackComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "MonkeyWrenchActor"

AMonkeyWrenchActor::AMonkeyWrenchActor()
{
	PrimaryActorTick.bCanEverTick = false;
	WorldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldMesh"));
	SetRootComponent(WorldMesh);
	WorldMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	WorldMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	WorldMesh->SetUseCCD(true);
	MeleeAttack = CreateDefaultSubobject<UMeleeAttackComponent>(TEXT("MeleeAttack"));
}

void AMonkeyWrenchActor::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	LastSafeTransform = InitialTransform;
}

void AMonkeyWrenchActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (MeleeAttack)
	{
		MeleeAttack->CancelAttack();
	}
	if (AActor* SlotActor = FixedSlot.Get())
	{
		if (IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(SlotActor))
		{
			Slot->NotifyAssignedPhysicalCarryItemEnding(*this);
		}
	}
	FixedSlot.Reset();
	if (Carrier)
	{
		Carrier->NotifyHeldActorEnding(this);
	}
	Carrier = nullptr;
	OnHeldPresentationChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

void AMonkeyWrenchActor::FellOutOfWorld(const UDamageType& DamageType)
{
	RecoverPhysicalCarryable(Carrier);
}

FPlayerInteractionQuery AMonkeyWrenchActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (Carrier || IsStoredInAssignedPhysicalCarryFixedSlot())
	{
		return Query;
	}
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("WrenchName", "몽키스패너");
	Query.ActionName = LOCTEXT("TakeWrench", "몽키스패너 들기");
	Query.bCanInteract = Context.CarryComponent && Context.CarryComponent->IsHandEmpty();
	if (!Query.bCanInteract)
	{
		Query.FailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
	}
	return Query;
}

FPlayerInteractionResult AMonkeyWrenchActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	FText FailureReason;
	return Context.CarryComponent && Context.CarryComponent->TryTakePhysicalObject(this, FailureReason)
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(FailureReason);
}

FText AMonkeyWrenchActor::GetPhysicalCarryDisplayName() const
{
	return LOCTEXT("WrenchName", "몽키스패너");
}

FTransform AMonkeyWrenchActor::GetHeldTransform() const
{
	FTransform Result = HeldTransform;
	Result.SetScale3D(FVector::OneVector);
	return Result;
}

bool AMonkeyWrenchActor::CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const
{
	if (IsStoredInAssignedPhysicalCarryFixedSlot())
	{
		OutFailureReason = LOCTEXT("TakeFromSlotRequired", "몽키스패너는 전용 슬롯에서 가져와야 합니다.");
		return false;
	}
	if (Carrier || !Carry.IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	return true;
}

bool AMonkeyWrenchActor::HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor)
{
	if (Carrier)
	{
		return false;
	}
	LastSafeTransform = GetActorTransform();
	Carrier = &Carry;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetWorldPhysics(false);
	if (HeldAnchor)
	{
		AttachToComponent(HeldAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		ApplyHeldTransform();
	}
	return true;
}

UPrimitiveComponent* AMonkeyWrenchActor::GetPhysicalCarryPrimitive() const
{
	return WorldMesh;
}

bool AMonkeyWrenchActor::CanFreeDrop(FText& OutFailureReason) const
{
	if (!Carrier || Carrier->GetHeldObject() != this)
	{
		OutFailureReason = LOCTEXT("WrenchNotHeldForDrop", "몽키스패너를 들고 있어야 내려놓을 수 있습니다.");
		return false;
	}
	return true;
}

bool AMonkeyWrenchActor::TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason)
{
	if (bFixedSlotBindingConflict)
	{
		OutFailureReason = LOCTEXT("SlotBindingConflict", "몽키스패너가 여러 슬롯에 연결되어 있습니다.");
		return false;
	}
	IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(&SlotActor);
	if (!Slot || Slot->GetAssignedPhysicalCarryItem() != this)
	{
		OutFailureReason = LOCTEXT("InvalidSlotBinding", "몽키스패너와 슬롯 연결이 올바르지 않습니다.");
		return false;
	}
	if (FixedSlot.IsValid() && FixedSlot.Get() != &SlotActor)
	{
		OutFailureReason = LOCTEXT("DuplicateSlotBinding", "몽키스패너가 여러 슬롯에 연결되어 있습니다.");
		return false;
	}
	FixedSlot = &SlotActor;
	return true;
}

void AMonkeyWrenchActor::ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot)
{
	if (FixedSlot.Get() == &ExpectedSlot)
	{
		FixedSlot.Reset();
	}
}

bool AMonkeyWrenchActor::IsStoredInAssignedPhysicalCarryFixedSlot() const
{
	const IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(FixedSlot.Get());
	return Slot && Slot->GetStoredPhysicalCarryItem() == this;
}

bool AMonkeyWrenchActor::NotifyTakenFromFixedSlotCommitted(
	UPlayerCarryComponent& Carry,
	AActor& SlotActor)
{
	if (FixedSlot.Get() != &SlotActor || Carrier)
	{
		return false;
	}
	Carrier = &Carry;
	return true;
}

bool AMonkeyWrenchActor::NotifyStoredInFixedSlotCommitted(
	UPlayerCarryComponent& Carry,
	AActor& SlotActor)
{
	if (FixedSlot.Get() != &SlotActor || Carrier != &Carry)
	{
		return false;
	}
	Carrier = nullptr;
	LastSafeTransform = GetActorTransform();
	return true;
}

bool AMonkeyWrenchActor::NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor)
{
	if (FixedSlot.Get() != &SlotActor || bEndingPlay)
	{
		return false;
	}
	if (Carrier && Carrier->GetHeldObject() == this)
	{
		return false;
	}
	Carrier = nullptr;
	LastSafeTransform = GetActorTransform();
	return true;
}

void AMonkeyWrenchActor::NotifyFixedSlotDestroyed(AActor& SlotActor)
{
	if (FixedSlot.Get() == &SlotActor)
	{
		FixedSlot.Reset();
		Carrier = nullptr;
		LastSafeTransform = GetActorTransform();
	}
}

bool AMonkeyWrenchActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	if (Carrier != &Carry)
	{
		return false;
	}
	Carrier = nullptr;
	LastSafeTransform = GetActorTransform();
	return true;
}

void AMonkeyWrenchActor::PublishPhysicalCarryCommit(const EPhysicalCarryCommitTransition Transition)
{
	OnHeldPresentationChanged.Broadcast(
		Transition == EPhysicalCarryCommitTransition::TakenIntoHand);
}

void AMonkeyWrenchActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
{
	if (Carrier && PreviousCarry && Carrier != PreviousCarry)
	{
		return;
	}
	if (Carrier && Carrier->GetHeldObject() == this
		&& Carrier->RecoverHeldPhysicalObject(this))
	{
		return;
	}
	if (MeleeAttack)
	{
		MeleeAttack->CancelAttack();
	}
	if (AActor* SlotActor = FixedSlot.Get())
	{
		if (IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(SlotActor))
		{
			if (Slot->TryRecoverAssignedPhysicalCarryItem(*this))
			{
				return;
			}
		}
	}
	Carrier = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetWorldPhysics(false);
	SetActorTransform(LastSafeTransform);
	SetWorldPhysics(true);
}

FHeldEquipmentUseQuery AMonkeyWrenchActor::QueryEquipmentUse(const FHeldEquipmentUseContext& Context) const
{
	FHeldEquipmentUseQuery Query;
	Query.DisplayName = GetPhysicalCarryDisplayName();
	Query.ActionName = LOCTEXT("Attack", "휘두르기");
	Query.ActivationMode = EPlayerInteractionActivationMode::Instant;
	Query.bCanUse = Carrier == Context.CarryComponent
		&& Context.CarryComponent
		&& Context.CarryComponent->GetHeldObject() == this
		&& MeleeAttack
		&& !MeleeAttack->IsAttacking();
	if (!Query.bCanUse)
	{
		Query.FailureReason = MeleeAttack && MeleeAttack->IsAttacking()
			? LOCTEXT("AttackInProgress", "이미 휘두르는 중입니다.")
			: LOCTEXT("WrenchNotHeld", "몽키스패너를 들고 있어야 합니다.");
	}
	return Query;
}

FHeldEquipmentUseResult AMonkeyWrenchActor::BeginEquipmentUse(const FHeldEquipmentUseContext& Context)
{
	const FHeldEquipmentUseQuery Query = QueryEquipmentUse(Context);
	if (!Query.bCanUse || !MeleeAttack || !MeleeAttack->StartAttack(Context.User, this, Context.Camera, Context.MotionComponent))
	{
		return FHeldEquipmentUseResult::Failed(
			Query.FailureReason.IsEmpty() ? LOCTEXT("AttackStartFailed", "공격을 시작할 수 없습니다.") : Query.FailureReason);
	}
	return FHeldEquipmentUseResult::Succeeded();
}

FHeldEquipmentUseUpdate AMonkeyWrenchActor::UpdateEquipmentUse(const FHeldEquipmentUseContext& Context, float DeltaTime)
{
	FHeldEquipmentUseUpdate Update;
	Update.State = EPlayerHoldInteractionState::Succeeded;
	Update.Progress = 1.0f;
	return Update;
}

FHeldEquipmentUseResult AMonkeyWrenchActor::EndEquipmentUse(const FHeldEquipmentUseContext& Context)
{
	return FHeldEquipmentUseResult::Succeeded();
}

void AMonkeyWrenchActor::CancelEquipmentUse(const FHeldEquipmentUseContext& Context)
{
	if (MeleeAttack)
	{
		MeleeAttack->CancelAttack();
	}
}

void AMonkeyWrenchActor::SetWorldPhysics(const bool bEnabled)
{
	if (!WorldMesh)
	{
		return;
	}
	if (bEnabled)
	{
		WorldMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		WorldMesh->SetUseCCD(true);
		WorldMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		WorldMesh->SetSimulatePhysics(true);
	}
	else
	{
		WorldMesh->SetSimulatePhysics(false);
		WorldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AMonkeyWrenchActor::ApplyHeldTransform()
{
	const FTransform LocalHeldTransform = GetHeldTransform();
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetRelativeLocationAndRotation(LocalHeldTransform.GetLocation(), LocalHeldTransform.GetRotation());
	}
}

#if WITH_EDITOR
EDataValidationResult AMonkeyWrenchActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!HeldTransform.GetScale3D().Equals(FVector::OneVector))
	{
		Context.AddWarning(LOCTEXT("HeldTransformScaleIgnored", "HeldTransform scale is ignored at runtime."));
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
