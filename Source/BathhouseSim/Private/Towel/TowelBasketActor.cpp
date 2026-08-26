#include "Towel/TowelBasketActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#include "Towel/TowelCirculationSubsystem.h"
#include "Towel/TowelInventoryComponent.h"
#include "Towel/Presentation/TowelStackVisualComponent.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "TowelBasketActor"

ATowelBasketActor::ATowelBasketActor()
{
	PrimaryActorTick.bCanEverTick = false;
	WorldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldMesh"));
	SetRootComponent(WorldMesh);
	WorldMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	WorldMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	WorldMesh->SetUseCCD(true);
	Inventory = CreateDefaultSubobject<UTowelInventoryComponent>(TEXT("TowelInventory"));
	Inventory->ConfigureDefaults(ETowelState::None, 0, 10);
	TowelPresentationVisual = CreateDefaultSubobject<UTowelStackVisualComponent>(TEXT("TowelPresentationVisual"));
	TowelPresentationVisual->SetupAttachment(WorldMesh);
}

void ATowelBasketActor::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	LastSafeTransform = InitialTransform;
	TowelPresentationVisual->BindInventorySource(Inventory);
}

void ATowelBasketActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	TowelPresentationVisual->UnbindInventorySource();
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
	if (!bContentsRecovered && EndPlayReason == EEndPlayReason::Destroyed && Inventory
		&& Inventory->GetSnapshot().Count > 0)
	{
		if (UTowelCirculationSubsystem* Subsystem = GetWorld()->GetSubsystem<UTowelCirculationSubsystem>())
		{
			Subsystem->RecoverInventory(Inventory);
			bContentsRecovered = true;
		}
	}
	OnHeldPresentationChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

void ATowelBasketActor::FellOutOfWorld(const UDamageType& DamageType)
{
	RecoverPhysicalCarryable(Carrier);
}

FPlayerInteractionQuery ATowelBasketActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (Carrier || IsStoredInAssignedPhysicalCarryFixedSlot())
	{
		return Query;
	}
	Query.bVisible = true;
	Query.TargetName = LOCTEXT("BasketTarget", "수건 바구니");
	Query.ActionName = LOCTEXT("TakeBasket", "바구니 들기");
	Query.bCanInteract = Context.CarryComponent && Context.CarryComponent->IsHandEmpty();
	if (!Query.bCanInteract)
	{
		Query.FailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
	}
	return Query;
}

FPlayerInteractionResult ATowelBasketActor::ExecuteInteraction(const FPlayerInteractionContext& Context)
{
	FText FailureReason;
	return Context.CarryComponent && Context.CarryComponent->TryTakePhysicalObject(this, FailureReason)
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(FailureReason);
}

FText ATowelBasketActor::GetPhysicalCarryDisplayName() const
{
	return LOCTEXT("BasketTarget", "수건 바구니");
}

FTransform ATowelBasketActor::GetHeldTransform() const
{
	FTransform Result = HeldTransform;
	Result.SetScale3D(FVector::OneVector);
	return Result;
}

bool ATowelBasketActor::CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const
{
	if (IsStoredInAssignedPhysicalCarryFixedSlot())
	{
		OutFailureReason = LOCTEXT("TakeFromSlotRequired", "수건 바구니는 전용 슬롯에서 가져와야 합니다.");
		return false;
	}
	if (Carrier || !Carry.IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	return true;
}

bool ATowelBasketActor::HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor)
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

UPrimitiveComponent* ATowelBasketActor::GetPhysicalCarryPrimitive() const
{
	return WorldMesh;
}

bool ATowelBasketActor::CanFreeDrop(FText& OutFailureReason) const
{
	if (!Carrier || Carrier->GetHeldObject() != this)
	{
		OutFailureReason = LOCTEXT("BasketNotHeldForDrop", "수건 바구니를 들고 있어야 내려놓을 수 있습니다.");
		return false;
	}
	return true;
}

bool ATowelBasketActor::TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason)
{
	if (bFixedSlotBindingConflict)
	{
		OutFailureReason = LOCTEXT("SlotBindingConflict", "수건 바구니가 여러 슬롯에 연결되어 있습니다.");
		return false;
	}
	IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(&SlotActor);
	if (!Slot || Slot->GetAssignedPhysicalCarryItem() != this)
	{
		OutFailureReason = LOCTEXT("InvalidSlotBinding", "수건 바구니와 슬롯 연결이 올바르지 않습니다.");
		return false;
	}
	if (FixedSlot.IsValid() && FixedSlot.Get() != &SlotActor)
	{
		OutFailureReason = LOCTEXT("DuplicateSlotBinding", "수건 바구니가 여러 슬롯에 연결되어 있습니다.");
		return false;
	}
	FixedSlot = &SlotActor;
	return true;
}

void ATowelBasketActor::ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot)
{
	if (FixedSlot.Get() == &ExpectedSlot)
	{
		FixedSlot.Reset();
	}
}

bool ATowelBasketActor::IsStoredInAssignedPhysicalCarryFixedSlot() const
{
	const IPhysicalCarryFixedSlot* Slot = Cast<IPhysicalCarryFixedSlot>(FixedSlot.Get());
	return Slot && Slot->GetStoredPhysicalCarryItem() == this;
}

bool ATowelBasketActor::NotifyTakenFromFixedSlotCommitted(
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

bool ATowelBasketActor::NotifyStoredInFixedSlotCommitted(
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

bool ATowelBasketActor::NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor)
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

void ATowelBasketActor::NotifyFixedSlotDestroyed(AActor& SlotActor)
{
	if (FixedSlot.Get() == &SlotActor)
	{
		FixedSlot.Reset();
		Carrier = nullptr;
		LastSafeTransform = GetActorTransform();
	}
}

bool ATowelBasketActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	if (Carrier != &Carry)
	{
		return false;
	}
	Carrier = nullptr;
	LastSafeTransform = GetActorTransform();
	return true;
}

void ATowelBasketActor::PublishPhysicalCarryCommit(const EPhysicalCarryCommitTransition Transition)
{
	OnHeldPresentationChanged.Broadcast(
		Transition == EPhysicalCarryCommitTransition::TakenIntoHand);
}

void ATowelBasketActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
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

void ATowelBasketActor::SetWorldPhysics(const bool bEnabled)
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

void ATowelBasketActor::ApplyHeldTransform()
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
EDataValidationResult ATowelBasketActor::IsDataValid(FDataValidationContext& Context) const
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
