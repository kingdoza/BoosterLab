#include "Cleaning/WetMopActor.h"

#include "Cleaning/WaterStainActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/HeldEquipmentMotionComponent.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "WetMopActor"

AWetMopActor::AWetMopActor()
{
	PrimaryActorTick.bCanEverTick = false;
	WorldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldMesh"));
	SetRootComponent(WorldMesh);
	WorldMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	WorldMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	WorldMesh->SetUseCCD(true);
}

void AWetMopActor::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	LastSafeTransform = InitialTransform;
}

void AWetMopActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	FHeldEquipmentUseContext UseContext;
	UseContext.User = Carrier ? Carrier->GetOwner() : nullptr;
	StopMopping(UseContext);
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
	OnMoppingStateChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

void AWetMopActor::FellOutOfWorld(const UDamageType& DamageType)
{
	RecoverPhysicalCarryable(Carrier);
}

FPlayerInteractionQuery AWetMopActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (Carrier || IsStoredInAssignedPhysicalCarryFixedSlot())
	{
		return Query;
	}
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("MopTarget", "물걸레");
	Query.ActionName = LOCTEXT("TakeMop", "물걸레 들기");
	Query.bCanInteract = Context.CarryComponent && Context.CarryComponent->IsHandEmpty();
	if (!Query.bCanInteract)
	{
		Query.FailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
	}
	return Query;
}

FPlayerInteractionResult AWetMopActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	FText FailureReason;
	return Context.CarryComponent && Context.CarryComponent->TryTakePhysicalObject(this, FailureReason)
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(FailureReason);
}

FText AWetMopActor::GetPhysicalCarryDisplayName() const
{
	return LOCTEXT("MopTarget", "물걸레");
}

FTransform AWetMopActor::GetHeldTransform() const
{
	FTransform Result = HeldTransform;
	Result.SetScale3D(FVector::OneVector);
	return Result;
}

bool AWetMopActor::CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const
{
	if (IsStoredInAssignedPhysicalCarryFixedSlot())
	{
		OutFailureReason = LOCTEXT("TakeFromSlotRequired", "물걸레는 전용 슬롯에서 가져와야 합니다.");
		return false;
	}
	if (Carrier || !Carry.IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	return true;
}

bool AWetMopActor::HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor)
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

UPrimitiveComponent* AWetMopActor::GetPhysicalCarryPrimitive() const
{
	return WorldMesh;
}

bool AWetMopActor::CanFreeDrop(FText& OutFailureReason) const
{
	if (!Carrier || Carrier->GetHeldObject() != this)
	{
		OutFailureReason = LOCTEXT("MopNotHeldForDrop", "물걸레를 들고 있어야 내려놓을 수 있습니다.");
		return false;
	}
	return true;
}

bool AWetMopActor::TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason)
{
	if (bFixedSlotBindingConflict)
	{
		OutFailureReason = LOCTEXT("SlotBindingConflict", "물걸레가 여러 슬롯에 연결되어 있습니다.");
		return false;
	}
	IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(&SlotActor);
	if (!Slot || Slot->GetAssignedPhysicalCarryItem() != this)
	{
		OutFailureReason = LOCTEXT("InvalidSlotBinding", "물걸레와 슬롯 연결이 올바르지 않습니다.");
		return false;
	}
	if (FixedSlot.IsValid() && FixedSlot.Get() != &SlotActor)
	{
		OutFailureReason = LOCTEXT("DuplicateSlotBinding", "물걸레가 여러 슬롯에 연결되어 있습니다.");
		return false;
	}
	FixedSlot = &SlotActor;
	return true;
}

void AWetMopActor::ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot)
{
	if (FixedSlot.Get() == &ExpectedSlot)
	{
		FixedSlot.Reset();
	}
}

bool AWetMopActor::IsStoredInAssignedPhysicalCarryFixedSlot() const
{
	const IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(FixedSlot.Get());
	return Slot && Slot->GetStoredPhysicalCarryItem() == this;
}

bool AWetMopActor::NotifyTakenFromFixedSlotCommitted(
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

bool AWetMopActor::NotifyStoredInFixedSlotCommitted(
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

bool AWetMopActor::NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor)
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

void AWetMopActor::NotifyFixedSlotDestroyed(AActor& SlotActor)
{
	if (FixedSlot.Get() != &SlotActor)
	{
		return;
	}
	FixedSlot.Reset();
	Carrier = nullptr;
	LastSafeTransform = GetActorTransform();
}

bool AWetMopActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	if (Carrier != &Carry)
	{
		return false;
	}
	Carrier = nullptr;
	LastSafeTransform = GetActorTransform();
	return true;
}

void AWetMopActor::PublishPhysicalCarryCommit(const EPhysicalCarryCommitTransition Transition)
{
	OnHeldPresentationChanged.Broadcast(
		Transition == EPhysicalCarryCommitTransition::TakenIntoHand);
}

FHeldEquipmentUseQuery AWetMopActor::QueryEquipmentUse(const FHeldEquipmentUseContext& Context) const
{
	FHeldEquipmentUseQuery Query;
	Query.DisplayName = LOCTEXT("MopTarget", "물걸레");
	Query.ActionName = LOCTEXT("MopUse", "물걸레질");
	Query.ActivationMode = EPlayerInteractionActivationMode::Hold;
	Query.bCanUse = Carrier == Context.CarryComponent
		&& Context.CarryComponent
		&& Context.CarryComponent->GetHeldObject() == this;
	if (!Query.bCanUse)
	{
		Query.FailureReason = LOCTEXT("MopNotHeld", "물걸레를 들고 있어야 합니다.");
		return Query;
	}
	if (const AWaterStainActor* Stain = ResolveFocusedStain(Context))
	{
		FText TargetFailure;
		float TargetProgress = 0.0f;
		Stain->QueryMopCleaning(Context.User, TargetFailure, TargetProgress);
		Query.Progress = TargetProgress;
		Query.FailureReason = TargetFailure;
	}
	else if (IsValid(ActiveStain))
	{
		Query.Progress = ActiveStain->GetCleaningProgress();
	}
	return Query;
}

FHeldEquipmentUseResult AWetMopActor::BeginEquipmentUse(const FHeldEquipmentUseContext& Context)
{
	const FHeldEquipmentUseQuery Query = QueryEquipmentUse(Context);
	if (!Query.bCanUse || !IsValid(Context.User))
	{
		return FHeldEquipmentUseResult::Failed(Query.FailureReason);
	}
	if (!bIsMopping)
	{
		bIsMopping = true;
		OnMoppingStateChanged.Broadcast(true);
	}
	if (Context.MotionComponent && GetRootComponent())
	{
		Context.MotionComponent->StartLoop(
			GetRootComponent(),
			MoppingPositionCurve,
			MoppingRotationCurve,
			FMath::Max(0.05f, MoppingMotionPeriodSeconds));
	}
	ChangeActiveStain(ResolveFocusedStain(Context), Context.User);
	return FHeldEquipmentUseResult::Succeeded();
}

FHeldEquipmentUseUpdate AWetMopActor::UpdateEquipmentUse(
	const FHeldEquipmentUseContext& Context,
	const float DeltaTime)
{
	FHeldEquipmentUseUpdate Update;
	if (!bIsMopping || Carrier != Context.CarryComponent
		|| !Context.CarryComponent || Context.CarryComponent->GetHeldObject() != this)
	{
		StopMopping(Context);
		Update.FailureReason = LOCTEXT("MoppingInvalidated", "물걸레 사용 상태가 유지되지 않았습니다.");
		return Update;
	}

	AWaterStainActor* FocusedStain = ResolveFocusedStain(Context);
	if (FocusedStain != ActiveStain)
	{
		ChangeActiveStain(FocusedStain, Context.User);
	}
	if (IsValid(ActiveStain))
	{
		Update = ActiveStain->UpdateMopCleaning(Context.User, DeltaTime);
		if (Update.State == EPlayerHoldInteractionState::Succeeded)
		{
			ActiveStain = nullptr;
			Update.State = EPlayerHoldInteractionState::Running;
			Update.Progress = 0.0f;
		}
		else if (Update.State == EPlayerHoldInteractionState::Failed)
		{
			ActiveStain = nullptr;
			Update.State = EPlayerHoldInteractionState::Running;
		}
		return Update;
	}
	Update.State = EPlayerHoldInteractionState::Running;
	Update.Progress = 0.0f;
	return Update;
}

FHeldEquipmentUseResult AWetMopActor::EndEquipmentUse(const FHeldEquipmentUseContext& Context)
{
	StopMopping(Context);
	return FHeldEquipmentUseResult::Succeeded();
}

void AWetMopActor::CancelEquipmentUse(const FHeldEquipmentUseContext& Context)
{
	StopMopping(Context);
}

void AWetMopActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
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
	FHeldEquipmentUseContext UseContext;
	UseContext.User = Carrier ? Carrier->GetOwner() : nullptr;
	StopMopping(UseContext);
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

AWaterStainActor* AWetMopActor::ResolveFocusedStain(const FHeldEquipmentUseContext& Context) const
{
	return Cast<AWaterStainActor>(Context.FocusHit.GetActor());
}

void AWetMopActor::ChangeActiveStain(AWaterStainActor* NewStain, AActor* Cleaner)
{
	if (ActiveStain == NewStain)
	{
		return;
	}
	if (IsValid(ActiveStain))
	{
		ActiveStain->CancelMopCleaning(Cleaner);
	}
	ActiveStain = nullptr;
	if (IsValid(NewStain))
	{
		FText FailureReason;
		if (NewStain->BeginMopCleaning(Cleaner, FailureReason))
		{
			ActiveStain = NewStain;
		}
	}
}

void AWetMopActor::StopMopping(const FHeldEquipmentUseContext& Context)
{
	if (IsValid(ActiveStain))
	{
		ActiveStain->CancelMopCleaning(Context.User);
	}
	ActiveStain = nullptr;
	if (Context.MotionComponent)
	{
		Context.MotionComponent->StopMotion();
	}
	if (bIsMopping)
	{
		bIsMopping = false;
		OnMoppingStateChanged.Broadcast(false);
	}
}

void AWetMopActor::SetWorldPhysics(const bool bEnabled)
{
	if (WorldMesh)
	{
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
}

void AWetMopActor::ApplyHeldTransform()
{
	const FTransform LocalHeldTransform = GetHeldTransform();
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetRelativeLocationAndRotation(
			LocalHeldTransform.GetLocation(),
			LocalHeldTransform.GetRotation());
	}
}

#if WITH_EDITOR
EDataValidationResult AWetMopActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!HeldTransform.GetScale3D().Equals(FVector::OneVector))
	{
		Context.AddWarning(LOCTEXT(
			"HeldTransformScaleIgnored",
			"HeldTransform scale is ignored at runtime. Author a unit scale and use location/rotation only."));
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
