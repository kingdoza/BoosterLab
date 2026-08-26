#include "Interaction/PhysicalCarryFixedSlotActor.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Interaction/PhysicalCarryPlacementTransaction.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerCarryComponent.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "PhysicalCarryFixedSlotActor"

APhysicalCarryFixedSlotActor::APhysicalCarryFixedSlotActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(SceneRoot);
	InteractionCollision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	ItemAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ItemAnchor"));
	ItemAnchor->SetupAttachment(SceneRoot);
	SlotDisplayName = LOCTEXT("DefaultSlotName", "물건 슬롯");
}

void APhysicalCarryFixedSlotActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeRuntimeSlot();
}

void APhysicalCarryFixedSlotActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		ReleaseStoredItemForSlotEndPlay();
	}
	if (AActor* Item = RuntimeAssignedItem.Get())
	{
		if (IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(Item))
		{
			Carryable->ClearPhysicalCarryFixedSlotBinding(*this);
		}
	}
	RuntimeAssignedItem = nullptr;
	bOccupied = false;
	bRuntimeOperational = false;
	OnSlotOccupancyChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

FPlayerInteractionQuery APhysicalCarryFixedSlotActor::QueryInteraction(
	const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	Query.bVisible = true;
	Query.TargetName = SlotDisplayName;
	Query.ActionName = bOccupied
		? LOCTEXT("TakeItem", "물건 가져가기")
		: LOCTEXT("StoreItem", "물건 놓기");

	if (!Context.CarryComponent)
	{
		Query.FailureReason = LOCTEXT("MissingCarry", "소지 상태를 확인할 수 없습니다.");
		return Query;
	}

	FText FailureReason;
	Query.bCanInteract = bOccupied
		? QueryTakePhysicalCarry(*Context.CarryComponent, FailureReason)
		: (Context.CarryComponent->GetHeldObject()
			&& QueryStorePhysicalCarry(
				*Context.CarryComponent,
				*Context.CarryComponent->GetHeldObject(),
				FailureReason));
	if (!bOccupied && !Context.CarryComponent->GetHeldObject())
	{
		FailureReason = LOCTEXT("NothingToStore", "놓을 물건이 없습니다.");
	}
	Query.FailureReason = FailureReason;
	return Query;
}

FPlayerInteractionResult APhysicalCarryFixedSlotActor::ExecuteInteraction(
	const FPlayerInteractionContext& Context)
{
	const FPlayerInteractionQuery Query = QueryInteraction(Context);
	if (!Query.bCanInteract || !Context.CarryComponent)
	{
		return FPlayerInteractionResult::Failed(Query.FailureReason);
	}
	return bOccupied
		? Context.CarryComponent->TryTakeFromFixedSlot(this)
		: Context.CarryComponent->TryStoreHeldObjectInFixedSlot(this);
}

AActor* APhysicalCarryFixedSlotActor::GetAssignedPhysicalCarryItem() const
{
	return bRuntimeInitialized ? RuntimeAssignedItem.Get() : AssignedItem.Get();
}

AActor* APhysicalCarryFixedSlotActor::GetStoredPhysicalCarryItem() const
{
	return bOccupied ? RuntimeAssignedItem.Get() : nullptr;
}

bool APhysicalCarryFixedSlotActor::IsPhysicalCarrySlotOperational(FText* OutFailureReason) const
{
	if (!bRuntimeOperational || !IsValid(RuntimeAssignedItem) || !IsValid(ItemAnchor))
	{
		if (OutFailureReason)
		{
			*OutFailureReason = RuntimeFailureReason.IsEmpty()
				? LOCTEXT("InvalidSlotState", "슬롯과 물건 연결 상태가 올바르지 않습니다.")
				: RuntimeFailureReason;
		}
		return false;
	}
	return true;
}

bool APhysicalCarryFixedSlotActor::QueryTakePhysicalCarry(
	const UPlayerCarryComponent& Carry,
	FText& OutFailureReason) const
{
	if (!IsPhysicalCarrySlotOperational(&OutFailureReason))
	{
		return false;
	}
	if (!bOccupied || !IsValid(RuntimeAssignedItem))
	{
		OutFailureReason = LOCTEXT("SlotEmpty", "슬롯에 물건이 없습니다.");
		return false;
	}
	if (!Carry.IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	return true;
}

bool APhysicalCarryFixedSlotActor::QueryStorePhysicalCarry(
	const UPlayerCarryComponent& Carry,
	const AActor& Item,
	FText& OutFailureReason) const
{
	if (!IsPhysicalCarrySlotOperational(&OutFailureReason))
	{
		return false;
	}
	if (bOccupied)
	{
		OutFailureReason = LOCTEXT("SlotOccupied", "슬롯에 이미 물건이 있습니다.");
		return false;
	}
	if (Carry.GetHeldObject() != &Item)
	{
		OutFailureReason = Carry.IsHandEmpty()
			? LOCTEXT("NothingToStore", "놓을 물건이 없습니다.")
			: LOCTEXT("HeldStateMismatch", "들고 있는 물건 상태가 변경되었습니다.");
		return false;
	}
	if (RuntimeAssignedItem != &Item)
	{
		OutFailureReason = LOCTEXT("WrongItem", "이 슬롯에 놓는 물건이 아닙니다.");
		return false;
	}
	return true;
}

bool APhysicalCarryFixedSlotActor::ApplyPhysicalCarrySlotOccupancy(
	AActor& ExpectedItem,
	const bool bInOccupied)
{
	if (!bRuntimeOperational || RuntimeAssignedItem != &ExpectedItem || bEndingPlay)
	{
		return false;
	}
	bOccupied = bInOccupied;
	return true;
}

void APhysicalCarryFixedSlotActor::NotifyPhysicalCarrySlotOccupancyCommitted()
{
	OnSlotOccupancyChanged.Broadcast(bOccupied);
}

bool APhysicalCarryFixedSlotActor::TryRecoverAssignedPhysicalCarryItem(AActor& ExpectedItem)
{
	if (!bRuntimeOperational || bOccupied || RuntimeAssignedItem != &ExpectedItem || bEndingPlay)
	{
		return false;
	}
	IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(&ExpectedItem);
	UPrimitiveComponent* Primitive = Carryable ? Carryable->GetPhysicalCarryPrimitive() : nullptr;
	if (!Carryable || !IsValid(Primitive) || Primitive != ExpectedItem.GetRootComponent() || !IsValid(ItemAnchor))
	{
		return false;
	}

	FPhysicalCarryPlacementTransaction Transaction(ExpectedItem, *Primitive, this);
	if (!Transaction.IsValid()
		|| !Transaction.ApplyFixedSlot(*ItemAnchor)
		|| !Transaction.ApplySlotOccupancy(true)
		|| !Carryable->NotifyRecoveredToFixedSlotCommitted(*this))
	{
		return false;
	}
	Transaction.Commit();
	NotifyPhysicalCarrySlotOccupancyCommitted();
	return true;
}

void APhysicalCarryFixedSlotActor::NotifyAssignedPhysicalCarryItemEnding(AActor& ExpectedItem)
{
	if (RuntimeAssignedItem != &ExpectedItem)
	{
		return;
	}
	const bool bWasOccupied = bOccupied;
	bOccupied = false;
	bRuntimeOperational = false;
	RuntimeFailureReason = LOCTEXT("AssignedItemEnding", "슬롯에 연결된 물건이 종료 중입니다.");
	RuntimeAssignedItem = nullptr;
	if (bWasOccupied && !bEndingPlay)
	{
		OnSlotOccupancyChanged.Broadcast(false);
	}
}

void APhysicalCarryFixedSlotActor::DisablePhysicalCarrySlot(const FText& FailureReason)
{
	if (!bRuntimeOperational && !RuntimeFailureReason.IsEmpty())
	{
		return;
	}
	if (bRuntimeOperational)
	{
		ReleaseStoredItemForSlotEndPlay();
	}
	RuntimeFailureReason = FailureReason.IsEmpty()
		? LOCTEXT("SlotDisabled", "슬롯을 사용할 수 없습니다.")
		: FailureReason;
	bRuntimeOperational = false;
	if (AActor* Item = RuntimeAssignedItem.Get())
	{
		if (IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(Item))
		{
			Carryable->ClearPhysicalCarryFixedSlotBinding(*this);
		}
	}
	RuntimeAssignedItem = nullptr;
}

bool APhysicalCarryFixedSlotActor::InitializeRuntimeSlot()
{
	bRuntimeInitialized = true;
	RuntimeAssignedItem = AssignedItem;
	FText FailureReason;
	if (!ValidateAssignment(FailureReason))
	{
		DisablePhysicalCarrySlot(FailureReason);
		return false;
	}

	IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(RuntimeAssignedItem.Get());
	if (!Carryable->TryBindPhysicalCarryFixedSlot(*this, FailureReason))
	{
		Carryable->NotifyPhysicalCarryFixedSlotBindingConflict();
		AActor* ExistingSlotActor = Carryable->GetAssignedPhysicalCarryFixedSlot();
		if (IPhysicalCarryFixedSlot* ExistingSlot = Cast<IPhysicalCarryFixedSlot>(ExistingSlotActor))
		{
			ExistingSlot->DisablePhysicalCarrySlot(
				LOCTEXT("DuplicateAssignment", "같은 물건이 여러 슬롯에 연결되어 있습니다."));
		}
		DisablePhysicalCarrySlot(
			LOCTEXT("DuplicateAssignment", "같은 물건이 여러 슬롯에 연결되어 있습니다."));
		return false;
	}

	bRuntimeOperational = true;
	bOccupied = false;
	if (bStartOccupied && !TryRecoverAssignedPhysicalCarryItem(*RuntimeAssignedItem))
	{
		DisablePhysicalCarrySlot(
			LOCTEXT("InitialOccupancyFailed", "슬롯의 초기 점유 상태를 적용할 수 없습니다."));
		return false;
	}
	return true;
}

bool APhysicalCarryFixedSlotActor::ValidateAssignment(FText& OutFailureReason) const
{
	AActor* Item = RuntimeAssignedItem.Get();
	if (!IsValid(Item))
	{
		OutFailureReason = LOCTEXT("MissingAssignedItem", "슬롯에 연결된 물건이 없습니다.");
		return false;
	}
	if (Item == this)
	{
		OutFailureReason = LOCTEXT("SelfAssignment", "슬롯은 자기 자신을 물건으로 연결할 수 없습니다.");
		return false;
	}
	const IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(Item);
	if (!Carryable
		|| !EnumHasAnyFlags(Carryable->GetPhysicalCarryCapabilities(), EPhysicalCarryCapability::FixedSlot))
	{
		OutFailureReason = LOCTEXT("UnsupportedAssignedItem", "연결된 물건은 고정 슬롯을 지원하지 않습니다.");
		return false;
	}
	if (Carryable->GetPhysicalCarryKind() == EPhysicalCarryKind::Key)
	{
		OutFailureReason = LOCTEXT("KeyRequiresHook", "키는 번호가 연결된 전용 키 걸이에만 배치할 수 있습니다.");
		return false;
	}
	UPrimitiveComponent* Primitive = Carryable->GetPhysicalCarryPrimitive();
	if (!IsValid(ItemAnchor) || !IsValid(Primitive) || Primitive != Item->GetRootComponent())
	{
		OutFailureReason = LOCTEXT("InvalidAssignedPrimitive", "연결된 물건의 물리 루트 또는 슬롯 앵커가 올바르지 않습니다.");
		return false;
	}
	return true;
}

void APhysicalCarryFixedSlotActor::ReleaseStoredItemForSlotEndPlay()
{
	AActor* Item = RuntimeAssignedItem.Get();
	IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(Item);
	UPrimitiveComponent* Primitive = Carryable ? Carryable->GetPhysicalCarryPrimitive() : nullptr;
	if (!bOccupied || !IsValid(Item) || Item->IsActorBeingDestroyed()
		|| !IsValid(Primitive) || Primitive != Item->GetRootComponent())
	{
		bOccupied = false;
		return;
	}

	const bool bWasEndingPlay = bEndingPlay;
	bEndingPlay = false;
	FPhysicalCarryPlacementTransaction Transaction(*Item, *Primitive, this);
	if (Transaction.IsValid()
		&& Transaction.ApplyFreeWorld(FVector::ZeroVector)
		&& Transaction.ApplySlotOccupancy(false))
	{
		Carryable->NotifyFixedSlotDestroyed(*this);
		Transaction.Commit();
		if (!bWasEndingPlay)
		{
			NotifyPhysicalCarrySlotOccupancyCommitted();
		}
	}
	bEndingPlay = bWasEndingPlay;
}

#if WITH_EDITOR
EDataValidationResult APhysicalCarryFixedSlotActor::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!IsTemplate())
	{
		if (!AssignedItem)
		{
			Context.AddError(LOCTEXT("ValidationMissingItem", "AssignedItem must reference one exact carryable Actor instance."));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			const IPhysicalCarryable* Carryable = Cast<IPhysicalCarryable>(AssignedItem.Get());
			if (!Carryable || !EnumHasAnyFlags(Carryable->GetPhysicalCarryCapabilities(), EPhysicalCarryCapability::FixedSlot))
			{
				Context.AddError(LOCTEXT("ValidationUnsupportedItem", "AssignedItem must implement the fixed-slot carry capability."));
				Result = EDataValidationResult::Invalid;
			}
			else if (Carryable->GetPhysicalCarryKind() == EPhysicalCarryKind::Key)
			{
				Context.AddError(LOCTEXT("ValidationKeyRequiresHook", "A bathhouse key must use its numbered BathhouseKeyHookActor, not a generic equipment slot."));
				Result = EDataValidationResult::Invalid;
			}
			else if (!Carryable->GetPhysicalCarryPrimitive()
				|| Carryable->GetPhysicalCarryPrimitive() != AssignedItem->GetRootComponent())
			{
				Context.AddError(LOCTEXT("ValidationInvalidRoot", "AssignedItem must expose its root primitive as the physical carry primitive."));
				Result = EDataValidationResult::Invalid;
			}
		}

		if (AssignedItem && GetWorld())
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				AActor* Other = *It;
				const IPhysicalCarryFixedSlot* OtherSlot = Other != this
					? Cast<IPhysicalCarryFixedSlot>(Other)
					: nullptr;
				if (OtherSlot && OtherSlot->GetAssignedPhysicalCarryItem() == AssignedItem)
				{
					Context.AddError(LOCTEXT("ValidationDuplicate", "AssignedItem is referenced by more than one physical carry fixed slot."));
					Result = EDataValidationResult::Invalid;
					break;
				}
			}
		}
	}
	if (!ItemAnchor || !InteractionCollision)
	{
		Context.AddError(LOCTEXT("ValidationMissingComponents", "ItemAnchor and InteractionCollision must be valid default subobjects."));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
