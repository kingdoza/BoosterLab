#include "Towel/TowelBasketActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Towel/TowelCirculationSubsystem.h"
#include "Towel/TowelInventoryComponent.h"

#define LOCTEXT_NAMESPACE "TowelBasketActor"

ATowelBasketActor::ATowelBasketActor()
{
	PrimaryActorTick.bCanEverTick = false;
	WorldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WorldMesh"));
	SetRootComponent(WorldMesh);
	WorldMesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Inventory = CreateDefaultSubobject<UTowelInventoryComponent>(TEXT("TowelInventory"));
	Inventory->ConfigureDefaults(ETowelState::None, 0, 10);
}

void ATowelBasketActor::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	LastSafeTransform = InitialTransform;
}

void ATowelBasketActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
	if (Carrier)
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

bool ATowelBasketActor::CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const
{
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
	}
	OnHeldPresentationChanged.Broadcast(true);
	return true;
}

UPrimitiveComponent* ATowelBasketActor::GetPhysicalCarryPrimitive() const
{
	return WorldMesh;
}

void ATowelBasketActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	ensureMsgf(Carrier == &Carry, TEXT("Towel basket drop committed by a carry component that does not own it."));
	Carrier = nullptr;
	LastSafeTransform = GetActorTransform();
	OnHeldPresentationChanged.Broadcast(false);
}

void ATowelBasketActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
{
	if (Carrier && PreviousCarry && Carrier != PreviousCarry)
	{
		return;
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

void ATowelBasketActor::SetWorldPhysics(const bool bEnabled)
{
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

#undef LOCTEXT_NAMESPACE
