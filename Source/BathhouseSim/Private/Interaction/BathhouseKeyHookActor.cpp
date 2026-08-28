#include "Interaction/BathhouseKeyHookActor.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Facility/BathhouseFacilitySubsystem.h"
#include "Interaction/BathhouseKeyActor.h"
#include "Interaction/PhysicalCarryPlacementTransaction.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerCarryComponent.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "BathhouseKeyHookActor"

ABathhouseKeyHookActor::ABathhouseKeyHookActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	KeyAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("KeyAnchor"));
	KeyAnchor->SetupAttachment(SceneRoot);
}

void ABathhouseKeyHookActor::BeginPlay()
{
	Super::BeginPlay();
	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>())
	{
		Subsystem->RegisterKeyHook(this, KeyNumber);
		KeyTopologyChangedHandle = Subsystem->OnKeyTopologyChanged.AddUObject(
			this,
			&ABathhouseKeyHookActor::HandleKeyTopologyChanged);
	}
	InitializeRuntimeFixedSlot();
}

void ABathhouseKeyHookActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		ReleaseStoredKeyForEndPlay();
	}
	if (KeyActor)
	{
		if (IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(KeyActor.Get()))
		{
			Carryable->ClearPhysicalCarryFixedSlotBinding(*this);
		}
	}
	if (UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr)
	{
		if (KeyTopologyChangedHandle.IsValid())
		{
			Subsystem->OnKeyTopologyChanged.Remove(KeyTopologyChangedHandle);
			KeyTopologyChangedHandle.Reset();
		}
		Subsystem->UnregisterKeyHook(this, KeyNumber);
	}
	bSlotOccupied = false;
	bRuntimeOperational = false;
	OnSlotOccupancyChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

FPlayerInteractionQuery ABathhouseKeyHookActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	Query.bVisible = true;
	Query.TargetName = GetPhysicalCarrySlotDisplayName();
	Query.ActionName = bSlotOccupied
		? LOCTEXT("TakeKey", "키 가져가기")
		: LOCTEXT("ReturnKey", "키 반환하기");
	if (!Context.CarryComponent)
	{
		Query.FailureReason = LOCTEXT("MissingCarry", "소지 상태를 확인할 수 없습니다.");
		return Query;
	}

	FText FailureReason;
	Query.bCanInteract = bSlotOccupied
		? QueryTakePhysicalCarry(*Context.CarryComponent, FailureReason)
		: (Context.CarryComponent->GetHeldObject()
			&& QueryStorePhysicalCarry(
				*Context.CarryComponent,
				*Context.CarryComponent->GetHeldObject(),
				FailureReason));
	if (!bSlotOccupied && !Context.CarryComponent->GetHeldObject())
	{
		FailureReason = LOCTEXT("NothingToStore", "놓을 물건이 없습니다.");
	}
	Query.FailureReason = FailureReason;
	return Query;
}

FPlayerInteractionResult ABathhouseKeyHookActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	const FPlayerInteractionQuery Query = QueryInteraction(Context);
	if (!Query.bCanInteract || !Context.CarryComponent)
	{
		return FPlayerInteractionResult::Failed(Query.FailureReason);
	}
	return bSlotOccupied
		? Context.CarryComponent->TryTakeFromFixedSlot(this)
		: Context.CarryComponent->TryStoreHeldObjectInFixedSlot(this);
}

AActor* ABathhouseKeyHookActor::GetAssignedPhysicalCarryItem() const
{
	return KeyActor.Get();
}

AActor* ABathhouseKeyHookActor::GetStoredPhysicalCarryItem() const
{
	return bSlotOccupied && IsValid(KeyActor) ? KeyActor.Get() : nullptr;
}

FText ABathhouseKeyHookActor::GetPhysicalCarrySlotDisplayName() const
{
	return FText::Format(LOCTEXT("HookTarget", "{0}번 키 걸이"), FText::AsNumber(KeyNumber));
}

bool ABathhouseKeyHookActor::IsPhysicalCarrySlotOperational(FText* OutFailureReason) const
{
	if (!bRuntimeOperational || !IsValid(KeyActor) || !IsValid(KeyAnchor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = RuntimeFailureReason.IsEmpty()
				? LOCTEXT("InvalidHookState", "키 걸이와 키 연결 상태가 올바르지 않습니다.")
				: RuntimeFailureReason;
		}
		return false;
	}
	return true;
}

bool ABathhouseKeyHookActor::QueryTakePhysicalCarry(
	const UPlayerCarryComponent& Carry,
	FText& OutFailureReason) const
{
	if (!IsPhysicalCarrySlotOperational(&OutFailureReason))
	{
		return false;
	}
	if (!bSlotOccupied || KeyActor->GetKeyState() != EBathhouseKeyState::AtHook)
	{
		OutFailureReason = LOCTEXT("KeyUnavailable", "키가 현재 걸이에 없습니다.");
		return false;
	}
	if (!Carry.IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	return true;
}

bool ABathhouseKeyHookActor::QueryStorePhysicalCarry(
	const UPlayerCarryComponent& Carry,
	const AActor& Item,
	FText& OutFailureReason) const
{
	if (!IsPhysicalCarrySlotOperational(&OutFailureReason))
	{
		return false;
	}
	if (bSlotOccupied)
	{
		OutFailureReason = LOCTEXT("HookOccupied", "키 걸이에 이미 키가 있습니다.");
		return false;
	}
	if (Carry.GetHeldObject() != &Item)
	{
		OutFailureReason = Carry.IsHandEmpty()
			? LOCTEXT("NothingToStore", "놓을 물건이 없습니다.")
			: LOCTEXT("HeldStateMismatch", "들고 있는 물건 상태가 변경되었습니다.");
		return false;
	}
	if (KeyActor != &Item)
	{
		OutFailureReason = LOCTEXT("WrongKey", "이 걸이에 반환할 키가 아닙니다.");
		return false;
	}
	if (KeyActor->GetKeyState() != EBathhouseKeyState::HeldByPlayer)
	{
		OutFailureReason = LOCTEXT("InvalidKeyState", "키 상태가 변경되어 반환할 수 없습니다.");
		return false;
	}
	return true;
}

bool ABathhouseKeyHookActor::ApplyPhysicalCarrySlotOccupancy(
	AActor& ExpectedItem,
	const bool bOccupied)
{
	if (!bRuntimeOperational || bEndingPlay || KeyActor != &ExpectedItem)
	{
		return false;
	}
	bSlotOccupied = bOccupied;
	return true;
}

void ABathhouseKeyHookActor::NotifyPhysicalCarrySlotOccupancyCommitted()
{
	OnSlotOccupancyChanged.Broadcast(bSlotOccupied);
}

bool ABathhouseKeyHookActor::TryRecoverAssignedPhysicalCarryItem(AActor& ExpectedItem)
{
	if (!bRuntimeOperational || bEndingPlay || KeyActor != &ExpectedItem)
	{
		return false;
	}
	if (bSlotOccupied)
	{
		if (GetStoredPhysicalCarryItem() == &ExpectedItem
			&& KeyActor->GetKeyState() == EBathhouseKeyState::AtHook)
		{
			return true;
		}
		// Recovery repairs a stale occupancy bit only for this exact assigned item.
		bSlotOccupied = false;
	}
	IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(&ExpectedItem);
	UPrimitiveComponent* Primitive = Carryable ? Carryable->GetPhysicalCarryPrimitive() : nullptr;
	if (!Carryable || !IsValid(Primitive) || Primitive != ExpectedItem.GetRootComponent() || !IsValid(KeyAnchor))
	{
		return false;
	}

	FPhysicalCarryPlacementTransaction Transaction(ExpectedItem, *Primitive, this);
	if (!Transaction.IsValid()
		|| !Transaction.ApplyFixedSlot(*KeyAnchor)
		|| !Transaction.ApplySlotOccupancy(true)
		|| !Carryable->NotifyRecoveredToFixedSlotCommitted(*this))
	{
		return false;
	}
	Transaction.Commit();
	NotifyPhysicalCarrySlotOccupancyCommitted();
	return true;
}

void ABathhouseKeyHookActor::NotifyAssignedPhysicalCarryItemEnding(AActor& ExpectedItem)
{
	if (KeyActor != &ExpectedItem)
	{
		return;
	}
	const bool bWasOccupied = bSlotOccupied;
	bSlotOccupied = false;
	bRuntimeOperational = false;
	RuntimeFailureReason = LOCTEXT("KeyEnding", "연결된 키가 종료 중입니다.");
	if (bWasOccupied && !bEndingPlay)
	{
		OnSlotOccupancyChanged.Broadcast(false);
	}
}

void ABathhouseKeyHookActor::DisablePhysicalCarrySlot(const FText& FailureReason)
{
	if (bRuntimeOperational)
	{
		ReleaseStoredKeyForEndPlay();
	}
	bRuntimeOperational = false;
	RuntimeFailureReason = FailureReason.IsEmpty()
		? LOCTEXT("HookDisabled", "키 걸이를 사용할 수 없습니다.")
		: FailureReason;
	if (KeyActor)
	{
		if (IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(KeyActor.Get()))
		{
			Carryable->ClearPhysicalCarryFixedSlotBinding(*this);
		}
	}
}

bool ABathhouseKeyHookActor::IsNumberTopologyValid(FText* OutFailureReason) const
{
	if (!KeyActor || KeyActor->GetKeyNumber() != KeyNumber)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = LOCTEXT("InvalidKeyLink", "키 걸이와 키 번호 연결이 올바르지 않습니다.");
		}
		return false;
	}

	const UBathhouseFacilitySubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UBathhouseFacilitySubsystem>() : nullptr;
	if (!Subsystem)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = LOCTEXT("MissingSubsystem", "시설 정보를 확인할 수 없습니다.");
		}
		return false;
	}
	return Subsystem->ValidateKeyNumber(KeyNumber, this, OutFailureReason);
}

bool ABathhouseKeyHookActor::InitializeRuntimeFixedSlot()
{
	FText FailureReason;
	if (!IsNumberTopologyValid(&FailureReason))
	{
		DisablePhysicalCarrySlot(
			FailureReason.IsEmpty()
				? LOCTEXT("HookInitializationFailed", "키 걸이 초기화에 실패했습니다.")
				: FailureReason);
		return false;
	}
	if (!KeyActor->InitializeAtHook(this))
	{
		if (IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(KeyActor.Get()))
		{
			Carryable->NotifyPhysicalCarryFixedSlotBindingConflict();
			AActor* ExistingSlotActor = Carryable->GetAssignedPhysicalCarryFixedSlot();
			if (IPhysicalCarryFixedSlot* ExistingSlot = Cast<IPhysicalCarryFixedSlot>(ExistingSlotActor))
			{
				ExistingSlot->DisablePhysicalCarrySlot(
					LOCTEXT("DuplicateKeyAssignment", "같은 키가 여러 고정 슬롯에 연결되어 있습니다."));
			}
		}
		DisablePhysicalCarrySlot(
			LOCTEXT("DuplicateKeyAssignment", "같은 키가 여러 고정 슬롯에 연결되어 있습니다."));
		return false;
	}
	bRuntimeOperational = true;
	RuntimeFailureReason = FText::GetEmpty();
	bSlotOccupied = KeyActor->GetKeyState() == EBathhouseKeyState::AtHook;
	return true;
}

void ABathhouseKeyHookActor::HandleKeyTopologyChanged()
{
	const UWorld* World = GetWorld();
	if (bEndingPlay || !World || World->bIsTearingDown)
	{
		return;
	}
	InitializeRuntimeFixedSlot();
}

void ABathhouseKeyHookActor::ReleaseStoredKeyForEndPlay()
{
	if (!bSlotOccupied || !IsValid(KeyActor) || KeyActor->IsActorBeingDestroyed())
	{
		bSlotOccupied = false;
		return;
	}
	IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(KeyActor.Get());
	UPrimitiveComponent* Primitive = Carryable ? Carryable->GetPhysicalCarryPrimitive() : nullptr;
	if (!Carryable || !IsValid(Primitive) || Primitive != KeyActor->GetRootComponent())
	{
		bSlotOccupied = false;
		return;
	}

	const bool bWasEndingPlay = bEndingPlay;
	bEndingPlay = false;
	FPhysicalCarryPlacementTransaction Transaction(*KeyActor, *Primitive, this);
	if (Transaction.IsValid()
		&& Transaction.ApplyFreeWorld(FVector::ZeroVector)
		&& Transaction.ApplySlotOccupancy(false))
	{
		Carryable->NotifyFixedSlotDestroyed(*this);
		Transaction.Commit();
	}
	bEndingPlay = bWasEndingPlay;
}

#if WITH_EDITOR
EDataValidationResult ABathhouseKeyHookActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!KeyActor || KeyActor->GetKeyNumber() != KeyNumber)
	{
		Context.AddError(LOCTEXT("ValidationInvalidKey", "KeyActor must reference the exact key with the same KeyNumber."));
		Result = EDataValidationResult::Invalid;
	}
	if (!KeyAnchor || !InteractionCollision)
	{
		Context.AddError(LOCTEXT("ValidationMissingComponents", "KeyAnchor and InteractionCollision must be valid default subobjects."));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
