#include "Combat/MonkeyWrenchActor.h"

#include "Combat/MeleeAttackComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/PlayerCarryComponent.h"
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
	if (MeleeAttack)
	{
		MeleeAttack->CancelAttack();
	}
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
	if (Carrier)
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
	OnHeldPresentationChanged.Broadcast(true);
	return true;
}

UPrimitiveComponent* AMonkeyWrenchActor::GetPhysicalCarryPrimitive() const
{
	return WorldMesh;
}

void AMonkeyWrenchActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	ensureMsgf(Carrier == &Carry, TEXT("Monkey wrench drop committed by a carry component that does not own it."));
	if (MeleeAttack)
	{
		MeleeAttack->CancelAttack();
	}
	Carrier = nullptr;
	LastSafeTransform = GetActorTransform();
	OnHeldPresentationChanged.Broadcast(false);
}

void AMonkeyWrenchActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
{
	if (Carrier && PreviousCarry && Carrier != PreviousCarry)
	{
		return;
	}
	if (MeleeAttack)
	{
		MeleeAttack->CancelAttack();
	}
	const bool bWasHeld = Carrier != nullptr;
	if (Carrier)
	{
		Carrier->CommitReleasePhysicalObject(this);
	}
	Carrier = nullptr;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetWorldPhysics(false);
	SetActorTransform(LastSafeTransform.Equals(FTransform::Identity) ? InitialTransform : LastSafeTransform);
	SetWorldPhysics(true);
	if (bWasHeld)
	{
		OnHeldPresentationChanged.Broadcast(false);
	}
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
