#include "Interaction/BathhouseKeyActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Facility/BathhouseCounterActor.h"
#include "Interaction/BathhouseKeyHookActor.h"
#include "Interaction/PlayerCarryComponent.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "BathhouseKeyActor"

ABathhouseKeyActor::ABathhouseKeyActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	WorldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldMesh"));
	WorldMesh->SetupAttachment(SceneRoot);
	WorldMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ABathhouseKeyActor::BeginPlay()
{
	Super::BeginPlay();
	if (KeyHook)
	{
		InitializeAtHook(KeyHook);
	}
}

void ABathhouseKeyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPlayerCarryComponent* Carry = Cast<UPlayerCarryComponent>(StateOwner.Get()))
	{
		Carry->CommitReleaseKey(this);
	}
	if (KeyState == EBathhouseKeyState::OnCounter && CounterOwner)
	{
		CounterOwner->TakeReturnedObject(this);
	}
	StateOwner = nullptr;
	CounterOwner = nullptr;
	CounterReturnSlotIndex = INDEX_NONE;
	Super::EndPlay(EndPlayReason);
}

FPlayerInteractionQuery ABathhouseKeyActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (KeyState != EBathhouseKeyState::OnCounter)
	{
		return Query;
	}

	Query.bVisible = true;
	Query.TargetName = FText::Format(LOCTEXT("KeyTarget", "{0}번 키"), FText::AsNumber(KeyNumber));
	Query.ActionName = LOCTEXT("TakeKey", "키 가져가기");
	Query.bCanInteract = Context.CarryComponent && Context.CarryComponent->IsHandEmpty();
	if (!Query.bCanInteract)
	{
		Query.FailureReason = LOCTEXT("HandOccupied", "이미 다른 키를 들고 있습니다.");
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
	OutFailureReason = LOCTEXT("KeyTransactionRequired", "키는 키걸이 또는 카운터에서 가져와야 합니다.");
	return false;
}

bool ABathhouseKeyActor::HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor)
{
	return KeyState == EBathhouseKeyState::HeldByPlayer;
}

bool ABathhouseKeyActor::CanFreeDrop(FText& OutFailureReason) const
{
	OutFailureReason = LOCTEXT("KeyHookOnly", "키는 원래 키걸이에만 반환할 수 있습니다.");
	return false;
}

void ABathhouseKeyActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
{
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
	KeyHook = InHook;
	if (KeyState == EBathhouseKeyState::AtHook || KeyState == EBathhouseKeyState::Recovering)
	{
		CommitState(EBathhouseKeyState::AtHook, InHook);
		AttachAtHook();
		return true;
	}
	return KeyHook == InHook;
}

bool ABathhouseKeyActor::TryTakeFromHook(UPlayerCarryComponent& Carry, ABathhouseKeyHookActor& Hook)
{
	if (KeyState != EBathhouseKeyState::AtHook || KeyHook != &Hook || StateOwner != &Hook || !Carry.IsHandEmpty()
		|| !Hook.IsNumberTopologyValid())
	{
		return false;
	}
	if (!Carry.CommitTakeKey(this))
	{
		return false;
	}

	CommitState(EBathhouseKeyState::HeldByPlayer, &Carry);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (USceneComponent* Anchor = Carry.GetHeldAnchor())
	{
		AttachToComponent(Anchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		ApplyHeldTransform();
	}
	SetActorHiddenInGame(false);
	SetWorldPresentation(true, false);
	return true;
}

bool ABathhouseKeyActor::TryReturnToHook(UPlayerCarryComponent& Carry, ABathhouseKeyHookActor& Hook)
{
	if (KeyState != EBathhouseKeyState::HeldByPlayer || StateOwner != &Carry || KeyHook != &Hook)
	{
		return false;
	}
	if (!Carry.CommitReleaseKey(this))
	{
		return false;
	}
	CommitState(EBathhouseKeyState::AtHook, &Hook);
	AttachAtHook();
	return true;
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
	ABathhouseCounterActor& Counter,
	const int32 ReturnSlotIndex,
	USceneComponent& ReturnSlot)
{
	if (KeyState != EBathhouseKeyState::AssignedToCustomer || StateOwner != &Customer)
	{
		return false;
	}
	if (!Counter.PlaceReturnedObject(&Customer, ReturnSlotIndex, this))
	{
		return false;
	}

	CounterOwner = &Counter;
	CounterReturnSlotIndex = ReturnSlotIndex;
	CommitState(EBathhouseKeyState::OnCounter, &Counter);
	AttachToComponent(&ReturnSlot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetWorldPresentation(true, true);
	return true;
}

bool ABathhouseKeyActor::TryTakeFromCounter(UPlayerCarryComponent& Carry)
{
	if (KeyState != EBathhouseKeyState::OnCounter || StateOwner != CounterOwner || !CounterOwner || !Carry.IsHandEmpty())
	{
		return false;
	}
	if (!CounterOwner->TakeReturnedObject(this))
	{
		return false;
	}
	if (!Carry.CommitTakeKey(this))
	{
		return false;
	}

	CounterOwner = nullptr;
	CounterReturnSlotIndex = INDEX_NONE;
	CommitState(EBathhouseKeyState::HeldByPlayer, &Carry);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (USceneComponent* Anchor = Carry.GetHeldAnchor())
	{
		AttachToComponent(Anchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		ApplyHeldTransform();
	}
	SetWorldPresentation(true, false);
	return true;
}

void ABathhouseKeyActor::RecoverToHook(UObject* ExpectedOwner)
{
	if (!KeyHook)
	{
		return;
	}

	UObject* PreviousOwner = StateOwner.Get();
	if (ExpectedOwner && PreviousOwner != ExpectedOwner)
	{
		return;
	}

	if (KeyState == EBathhouseKeyState::AtHook && PreviousOwner == KeyHook && !CounterOwner)
	{
		AttachAtHook();
		return;
	}

	CommitState(EBathhouseKeyState::Recovering, this);
	if (UPlayerCarryComponent* Carry = Cast<UPlayerCarryComponent>(PreviousOwner))
	{
		Carry->CommitReleaseKey(this);
	}
	if (CounterOwner)
	{
		CounterOwner->TakeReturnedObject(this);
		CounterOwner = nullptr;
		CounterReturnSlotIndex = INDEX_NONE;
	}
	CommitState(EBathhouseKeyState::AtHook, KeyHook);
	AttachAtHook();
}

bool ABathhouseKeyActor::CommitState(const EBathhouseKeyState NewState, UObject* NewOwner)
{
	const EBathhouseKeyState PreviousState = KeyState;
	const bool bWasHeld = PreviousState == EBathhouseKeyState::HeldByPlayer;
	KeyState = NewState;
	StateOwner = NewOwner;
	if (PreviousState != NewState)
	{
		OnKeyStateChanged.Broadcast(PreviousState, NewState);
		const bool bIsHeld = NewState == EBathhouseKeyState::HeldByPlayer;
		if (bWasHeld != bIsHeld)
		{
			OnHeldPresentationChanged.Broadcast(bIsHeld);
		}
	}
	return true;
}

void ABathhouseKeyActor::AttachAtHook()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (KeyHook && KeyHook->GetKeyAnchor())
	{
		AttachToComponent(KeyHook->GetKeyAnchor(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	SetActorHiddenInGame(false);
	SetWorldPresentation(true, false);
}

void ABathhouseKeyActor::SetWorldPresentation(const bool bVisible, const bool bCollisionEnabled)
{
	SetActorHiddenInGame(!bVisible);
	if (WorldMesh)
	{
		WorldMesh->SetCollisionEnabled(bCollisionEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
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
