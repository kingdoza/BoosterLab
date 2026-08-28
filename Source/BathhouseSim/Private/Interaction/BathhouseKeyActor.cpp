#include "Interaction/BathhouseKeyActor.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Facility/BathhouseCounterActor.h"
#include "Interaction/BathhouseKeyHookActor.h"
#include "Interaction/CheckoutKeyPlacementUtils.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#include "Interaction/PlayerCarryComponent.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "BathhouseKeyActor"

ABathhouseKeyActor::ABathhouseKeyActor()
{
	PrimaryActorTick.bCanEverTick = false;
	KeyPhysicsRoot = CreateDefaultSubobject<UBoxComponent>(TEXT("KeyPhysicsRoot"));
	SetRootComponent(KeyPhysicsRoot);
	KeyPhysicsRoot->SetBoxExtent(FVector(8.0f, 4.0f, 2.0f));
	KeyPhysicsRoot->SetCollisionProfileName(TEXT("PhysicsActor"));
	KeyPhysicsRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	KeyPhysicsRoot->SetUseCCD(true);
	KeyPhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetupAttachment(KeyPhysicsRoot);
	WorldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldMesh"));
	WorldMesh->SetupAttachment(SceneRoot);
	WorldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABathhouseKeyActor::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	LastSafeTransform = InitialTransform;
	if (KeyHook)
	{
		InitializeAtHook(KeyHook);
	}
}

void ABathhouseKeyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (AActor* SlotActor = FixedSlot.Get())
	{
		if (IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(SlotActor))
		{
			Slot->NotifyAssignedPhysicalCarryItemEnding(*this);
		}
	}
	FixedSlot.Reset();
	if (UPlayerCarryComponent* Carry = Cast<UPlayerCarryComponent>(StateOwner.Get()))
	{
		Carry->CommitReleaseKey(this);
	}
	if (KeyState == EBathhouseKeyState::OnCounter && CounterOwner)
	{
		CounterOwner = nullptr;
	}
	StateOwner = nullptr;
	CounterOwner = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ABathhouseKeyActor::FellOutOfWorld(const UDamageType& DamageType)
{
	RecoverPhysicalCarryable(Cast<UPlayerCarryComponent>(StateOwner.Get()));
}

FPlayerInteractionQuery ABathhouseKeyActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (KeyState != EBathhouseKeyState::OnCounter && KeyState != EBathhouseKeyState::DroppedInWorld)
	{
		return Query;
	}

	Query.bVisible = true;
	Query.TargetName = FText::Format(LOCTEXT("KeyTarget", "{0}번 키"), FText::AsNumber(KeyNumber));
	Query.ActionName = LOCTEXT("TakeKey", "키 가져가기");
	Query.bCanInteract = Context.CarryComponent && Context.CarryComponent->IsHandEmpty();
	if (!Query.bCanInteract)
	{
		Query.FailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
	}
	return Query;
}

FPlayerInteractionResult ABathhouseKeyActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	const FPlayerInteractionQuery Query = QueryInteraction(Context);
	if (!Query.bCanInteract || !Context.CarryComponent)
	{
		return FPlayerInteractionResult::Failed(Query.FailureReason);
	}
	if (KeyState == EBathhouseKeyState::DroppedInWorld)
	{
		FText FailureReason;
		return Context.CarryComponent->TryTakePhysicalObject(this, FailureReason)
			? FPlayerInteractionResult::Succeeded()
			: FPlayerInteractionResult::Failed(FailureReason);
	}
	return TryTakeFromCounter(*Context.CarryComponent)
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(LOCTEXT("TakeFailed", "키를 가져올 수 없습니다."));
}

FText ABathhouseKeyActor::GetPhysicalCarryDisplayName() const
{
	return FText::Format(LOCTEXT("KeyTarget", "{0}번 키"), FText::AsNumber(KeyNumber));
}

FTransform ABathhouseKeyActor::GetHeldTransform() const
{
	FTransform Result = HeldTransform;
	Result.SetScale3D(FVector::OneVector);
	return Result;
}

bool ABathhouseKeyActor::CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const
{
	if (KeyState != EBathhouseKeyState::DroppedInWorld)
	{
		OutFailureReason = LOCTEXT("KeyTransactionRequired", "키는 키걸이, 카운터 또는 월드 드랍 위치에서 가져와야 합니다.");
		return false;
	}
	if (!Carry.IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	return true;
}

bool ABathhouseKeyActor::HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor)
{
	if (KeyState != EBathhouseKeyState::DroppedInWorld || !IsValid(HeldAnchor))
	{
		return false;
	}
	LastSafeTransform = GetActorTransform();
	CommitState(EBathhouseKeyState::HeldByPlayer, &Carry, true);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetWorldPhysics(false);
	AttachToComponent(HeldAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	ApplyHeldTransform();
	SetActorHiddenInGame(false);
	return true;
}

bool ABathhouseKeyActor::CanFreeDrop(FText& OutFailureReason) const
{
	const UPlayerCarryComponent* Carry = Cast<UPlayerCarryComponent>(StateOwner.Get());
	if (KeyState != EBathhouseKeyState::HeldByPlayer || !Carry || Carry->GetHeldObject() != this)
	{
		OutFailureReason = LOCTEXT("KeyNotHeldForDrop", "키를 들고 있어야 내려놓을 수 있습니다.");
		return false;
	}
	return true;
}

UPrimitiveComponent* ABathhouseKeyActor::GetPhysicalCarryPrimitive() const
{
	return KeyPhysicsRoot;
}

bool ABathhouseKeyActor::TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason)
{
	if (bFixedSlotBindingConflict)
	{
		OutFailureReason = LOCTEXT("SlotBindingConflict", "키가 여러 고정 슬롯에 연결되어 있습니다.");
		return false;
	}
	IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(&SlotActor);
	if (!Slot || Slot->GetAssignedPhysicalCarryItem() != this)
	{
		OutFailureReason = LOCTEXT("InvalidSlotBinding", "키와 키걸이 연결이 올바르지 않습니다.");
		return false;
	}
	if (FixedSlot.IsValid() && FixedSlot.Get() != &SlotActor)
	{
		OutFailureReason = LOCTEXT("DuplicateSlotBinding", "키가 여러 고정 슬롯에 연결되어 있습니다.");
		return false;
	}
	FixedSlot = &SlotActor;
	return true;
}

void ABathhouseKeyActor::ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot)
{
	if (FixedSlot.Get() == &ExpectedSlot)
	{
		FixedSlot.Reset();
	}
}

bool ABathhouseKeyActor::IsStoredInAssignedPhysicalCarryFixedSlot() const
{
	const IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(FixedSlot.Get());
	return Slot && Slot->GetStoredPhysicalCarryItem() == this;
}

bool ABathhouseKeyActor::NotifyTakenFromFixedSlotCommitted(
	UPlayerCarryComponent& Carry,
	AActor& SlotActor)
{
	if (FixedSlot.Get() != &SlotActor || KeyState != EBathhouseKeyState::AtHook || StateOwner != &SlotActor)
	{
		return false;
	}
	CommitState(EBathhouseKeyState::HeldByPlayer, &Carry, true);
	SetActorHiddenInGame(false);
	return true;
}

bool ABathhouseKeyActor::NotifyStoredInFixedSlotCommitted(
	UPlayerCarryComponent& Carry,
	AActor& SlotActor)
{
	if (FixedSlot.Get() != &SlotActor || KeyState != EBathhouseKeyState::HeldByPlayer || StateOwner != &Carry)
	{
		return false;
	}
	CommitState(EBathhouseKeyState::AtHook, &SlotActor, true);
	LastSafeTransform = GetActorTransform();
	SetWorldPresentation(true, false);
	return true;
}

bool ABathhouseKeyActor::NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor)
{
	if (FixedSlot.Get() != &SlotActor || bEndingPlay)
	{
		return false;
	}
	if (UPlayerCarryComponent* Carry = Cast<UPlayerCarryComponent>(StateOwner.Get()))
	{
		if (Carry->GetHeldObject() == this)
		{
			return false;
		}
	}
	CommitState(EBathhouseKeyState::AtHook, &SlotActor, true);
	LastSafeTransform = GetActorTransform();
	SetWorldPresentation(true, false);
	return true;
}

void ABathhouseKeyActor::NotifyFixedSlotDestroyed(AActor& SlotActor)
{
	if (FixedSlot.Get() != &SlotActor)
	{
		return;
	}
	FixedSlot.Reset();
	KeyHook = nullptr;
	CommitState(EBathhouseKeyState::DroppedInWorld, nullptr);
	LastSafeTransform = GetActorTransform();
}

bool ABathhouseKeyActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	if (KeyState != EBathhouseKeyState::HeldByPlayer || StateOwner != &Carry)
	{
		return false;
	}
	CommitState(EBathhouseKeyState::DroppedInWorld, nullptr, true);
	LastSafeTransform = GetActorTransform();
	return true;
}

void ABathhouseKeyActor::PublishPhysicalCarryCommit(const EPhysicalCarryCommitTransition Transition)
{
	(void)Transition;
	if (!bHasDeferredStatePublication)
	{
		return;
	}
	const EBathhouseKeyState PreviousState = DeferredPreviousState;
	const EBathhouseKeyState NewState = DeferredNewState;
	bHasDeferredStatePublication = false;
	BroadcastStateTransition(PreviousState, NewState);
}

void ABathhouseKeyActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
{
	if (PreviousCarry && PreviousCarry->GetHeldObject() == this
		&& PreviousCarry->RecoverHeldPhysicalObject(this))
	{
		return;
	}
	RecoverToHook(PreviousCarry);
}

bool ABathhouseKeyActor::InitializeAtHook(ABathhouseKeyHookActor* InHook)
{
	if (!IsValid(InHook) || InHook->GetKeyNumber() != KeyNumber)
	{
		return false;
	}
	if (KeyHook && KeyHook != InHook)
	{
		return false;
	}
	if (!InHook->KeyActor && !GetWorld() && !InHook->GetWorld())
	{
		// Preserve native transient/unit construction compatibility. Runtime worlds
		// still require the exact reflected KeyActor assignment and validation.
		InHook->KeyActor = this;
		InHook->bRuntimeOperational = true;
		InHook->bSlotOccupied = true;
	}
	KeyHook = InHook;
	FText FailureReason;
	if (!TryBindPhysicalCarryFixedSlot(*InHook, FailureReason))
	{
		return false;
	}
	if (KeyState == EBathhouseKeyState::AtHook || KeyState == EBathhouseKeyState::Recovering)
	{
		CommitState(EBathhouseKeyState::AtHook, InHook);
		AttachAtHook();
		return true;
	}
	return FixedSlot.Get() == InHook;
}

bool ABathhouseKeyActor::TryTakeFromHook(UPlayerCarryComponent& Carry, ABathhouseKeyHookActor& Hook)
{
	return Carry.TryTakeFromFixedSlot(&Hook).bSucceeded;
}

bool ABathhouseKeyActor::TryReturnToHook(UPlayerCarryComponent& Carry, ABathhouseKeyHookActor& Hook)
{
	return Carry.TryStoreHeldObjectInFixedSlot(&Hook).bSucceeded;
}

bool ABathhouseKeyActor::TryAssignToCustomer(UPlayerCarryComponent& Carry, AActor& Customer)
{
	if (KeyState != EBathhouseKeyState::HeldByPlayer || StateOwner != &Carry || !IsValid(&Customer))
	{
		return false;
	}
	if (!Carry.CommitReleaseKey(this))
	{
		return false;
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	CommitState(EBathhouseKeyState::AssignedToCustomer, &Customer);
	SetWorldPresentation(false, false);
	return true;
}

bool ABathhouseKeyActor::TryPlaceOnCounter(
	AActor& Customer,
	ABathhouseCounterActor& Counter)
{
	if (KeyState == EBathhouseKeyState::OnCounter)
	{
		return StateOwner == &Counter && CounterOwner == &Counter;
	}
	if (KeyState != EBathhouseKeyState::AssignedToCustomer || StateOwner != &Customer)
	{
		return false;
	}
	if (!BathhouseCheckoutKeyPlacement::TryPlaceKeyInFreeWorld(*this, Counter))
	{
		return false;
	}

	CounterOwner = &Counter;
	CommitState(EBathhouseKeyState::OnCounter, &Counter);
	LastSafeTransform = GetActorTransform();
	SetActorHiddenInGame(false);
	Counter.NotifyReturnedKeyDropped(this);
	return true;
}

bool ABathhouseKeyActor::TryPlaceOnCounter(
	AActor& Customer,
	ABathhouseCounterActor& Counter,
	const int32 ReturnSlotIndex,
	USceneComponent& ReturnSlot)
{
	return TryPlaceOnCounter(Customer, Counter);
}

bool ABathhouseKeyActor::TryTakeFromCounter(UPlayerCarryComponent& Carry)
{
	if (KeyState != EBathhouseKeyState::OnCounter || StateOwner != CounterOwner || !CounterOwner || !Carry.IsHandEmpty())
	{
		return false;
	}
	if (!Carry.CommitTakeKey(this))
	{
		return false;
	}

	CounterOwner = nullptr;
	CommitState(EBathhouseKeyState::HeldByPlayer, &Carry);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetWorldPhysics(false);
	if (USceneComponent* Anchor = Carry.GetHeldAnchor())
	{
		AttachToComponent(Anchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		ApplyHeldTransform();
	}
	SetActorHiddenInGame(false);
	return true;
}

void ABathhouseKeyActor::RecoverToHook(UObject* ExpectedOwner)
{
	UObject* PreviousOwner = StateOwner.Get();
	if (ExpectedOwner && PreviousOwner != ExpectedOwner)
	{
		return;
	}
	if (KeyState == EBathhouseKeyState::AtHook && PreviousOwner == KeyHook
		&& IsStoredInAssignedPhysicalCarryFixedSlot() && !CounterOwner)
	{
		AttachAtHook();
		return;
	}

	CommitState(EBathhouseKeyState::Recovering, this);
	if (UPlayerCarryComponent* Carry = Cast<UPlayerCarryComponent>(PreviousOwner);
		Carry && Carry->GetHeldObject() == this)
	{
		Carry->CommitReleaseKey(this);
	}
	if (CounterOwner)
	{
		CounterOwner = nullptr;
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

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetWorldPhysics(false);
	SetActorTransform(LastSafeTransform);
	SetWorldPhysics(true);
	CommitState(EBathhouseKeyState::DroppedInWorld, nullptr);
}

bool ABathhouseKeyActor::CommitState(
	const EBathhouseKeyState NewState,
	UObject* NewOwner,
	const bool bDeferPublication)
{
	const EBathhouseKeyState PreviousState = KeyState;
	KeyState = NewState;
	StateOwner = NewOwner;
	if (PreviousState != NewState)
	{
		const EBathhouseKeyState PublicationPreviousState = bHasDeferredStatePublication
			? DeferredPreviousState
			: PreviousState;
		if (bDeferPublication)
		{
			DeferredPreviousState = PublicationPreviousState;
			DeferredNewState = NewState;
			bHasDeferredStatePublication = true;
		}
		else
		{
			bHasDeferredStatePublication = false;
			BroadcastStateTransition(PublicationPreviousState, NewState);
		}
	}
	return true;
}

void ABathhouseKeyActor::BroadcastStateTransition(
	const EBathhouseKeyState PreviousState,
	const EBathhouseKeyState NewState)
{
	if (PreviousState == NewState)
	{
		return;
	}
	OnKeyStateChanged.Broadcast(PreviousState, NewState);
	const bool bWasHeld = PreviousState == EBathhouseKeyState::HeldByPlayer;
	const bool bIsHeld = NewState == EBathhouseKeyState::HeldByPlayer;
	if (bWasHeld != bIsHeld)
	{
		OnHeldPresentationChanged.Broadcast(bIsHeld);
	}
}

void ABathhouseKeyActor::AttachAtHook()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetWorldPhysics(false);
	if (KeyHook && KeyHook->GetKeyAnchor())
	{
		AttachToComponent(KeyHook->GetKeyAnchor(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	SetActorHiddenInGame(false);
}

void ABathhouseKeyActor::SetWorldPresentation(const bool bVisible, const bool bCollisionEnabled)
{
	SetActorHiddenInGame(!bVisible);
	if (KeyPhysicsRoot)
	{
		KeyPhysicsRoot->SetSimulatePhysics(false);
		KeyPhysicsRoot->SetCollisionEnabled(
			bCollisionEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void ABathhouseKeyActor::SetWorldPhysics(const bool bEnabled)
{
	if (!KeyPhysicsRoot)
	{
		return;
	}
	if (bEnabled)
	{
		KeyPhysicsRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		KeyPhysicsRoot->SetUseCCD(true);
		KeyPhysicsRoot->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		KeyPhysicsRoot->SetSimulatePhysics(true);
	}
	else
	{
		KeyPhysicsRoot->SetSimulatePhysics(false);
		KeyPhysicsRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ABathhouseKeyActor::ApplyHeldTransform()
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
EDataValidationResult ABathhouseKeyActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!KeyPhysicsRoot || GetRootComponent() != KeyPhysicsRoot)
	{
		Context.AddError(LOCTEXT("InvalidPhysicsRoot", "KeyPhysicsRoot must remain the key Actor root component."));
		Result = EDataValidationResult::Invalid;
	}
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
