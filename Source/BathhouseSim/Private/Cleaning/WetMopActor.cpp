#include "Cleaning/WetMopActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/PlayerCarryComponent.h"
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
}

void AWetMopActor::BeginPlay()
{
	Super::BeginPlay();
	InitialTransform = GetActorTransform();
	LastSafeTransform = InitialTransform;
}

void AWetMopActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Carrier)
	{
		Carrier->NotifyHeldActorEnding(this);
	}
	Carrier = nullptr;
	OnHeldPresentationChanged.Clear();
	Super::EndPlay(EndPlayReason);
}

void AWetMopActor::FellOutOfWorld(const UDamageType& DamageType)
{
	RecoverPhysicalCarryable(Carrier);
}

FPlayerInteractionQuery AWetMopActor::QueryInteraction(const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (Carrier)
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
	OnHeldPresentationChanged.Broadcast(true);
	return true;
}

UPrimitiveComponent* AWetMopActor::GetPhysicalCarryPrimitive() const
{
	return WorldMesh;
}

void AWetMopActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	ensureMsgf(Carrier == &Carry, TEXT("Wet mop drop committed by a carry component that does not own it."));
	Carrier = nullptr;
	LastSafeTransform = GetActorTransform();
	OnHeldPresentationChanged.Broadcast(false);
}

void AWetMopActor::RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry)
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

void AWetMopActor::SetWorldPhysics(const bool bEnabled)
{
	if (WorldMesh)
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
