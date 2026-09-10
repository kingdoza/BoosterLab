#include "Placement/PlaceableFacilityItemActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Interaction/PlayerCarryComponent.h"
#include "Placement/FacilityPlacementDefinition.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "PlaceableFacilityItemActor"

APlaceableFacilityItemActor::APlaceableFacilityItemActor()
{
	PrimaryActorTick.bCanEverTick = false;
	ItemRoot = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemRoot"));
	SetRootComponent(ItemRoot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		ItemRoot->SetStaticMesh(Cube.Object);
	}
	ItemRoot->SetCollisionProfileName(TEXT("PhysicsActor"));
	ItemRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	ItemRoot->SetUseCCD(true);
	ItemRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ItemRoot->SetSimulatePhysics(false);
}

void APlaceableFacilityItemActor::BeginPlay()
{
	Super::BeginPlay();
	if (Lifecycle == ELifecycle::FreeWorld)
	{
		LastSafeTransform = GetActorTransform();
	}
}

void APlaceableFacilityItemActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Lifecycle != ELifecycle::PlacementConsumption
		&& Lifecycle != ELifecycle::PlacementConsumed)
	{
		if (UPlayerCarryComponent* LiveCarrier = Carrier.Get())
		{
			LiveCarrier->NotifyHeldActorEnding(this);
		}
	}
	Carrier.Reset();
	Payload.Reset();
	Super::EndPlay(EndPlayReason);
}

void APlaceableFacilityItemActor::FellOutOfWorld(const UDamageType& DamageType)
{
	RecoverPhysicalCarryable(Carrier.Get());
}

FPlayerInteractionQuery APlaceableFacilityItemActor::QueryInteraction(
	const FPlayerInteractionContext& Context) const
{
	FPlayerInteractionQuery Query;
	if (Lifecycle != ELifecycle::FreeWorld || Carrier.IsValid())
	{
		return Query;
	}
	Query.bVisible = true;
	Query.TargetName = GetPhysicalCarryDisplayName();
	Query.ActionName = LOCTEXT("TakeFacilityItem", "설비 아이템 들기");
	FText FailureReason;
	Query.bCanInteract = Context.CarryComponent
		&& CanBeTakenBy(*Context.CarryComponent, FailureReason);
	Query.FailureReason = FailureReason;
	return Query;
}

FPlayerInteractionResult APlaceableFacilityItemActor::ExecuteInteraction(
	const FPlayerInteractionContext& Context)
{
	FText FailureReason;
	return Context.CarryComponent
		&& Context.CarryComponent->TryTakePhysicalObject(this, FailureReason)
		? FPlayerInteractionResult::Succeeded()
		: FPlayerInteractionResult::Failed(
			FailureReason.IsEmpty()
				? LOCTEXT("TakeFacilityItemFailed", "설비 아이템을 들 수 없습니다.")
				: FailureReason);
}

FText APlaceableFacilityItemActor::GetPhysicalCarryDisplayName() const
{
	return LOCTEXT("FacilityItemName", "설비 회수 아이템");
}

FTransform APlaceableFacilityItemActor::GetHeldTransform() const
{
	FTransform Result = HeldTransform;
	Result.SetScale3D(FVector::OneVector);
	return Result;
}

bool APlaceableFacilityItemActor::CanBeTakenBy(
	const UPlayerCarryComponent& Carry,
	FText& OutFailureReason) const
{
	if (Lifecycle != ELifecycle::FreeWorld || Carrier.IsValid())
	{
		OutFailureReason = LOCTEXT("FacilityItemUnavailable", "이 설비 아이템은 현재 들 수 없습니다.");
		return false;
	}
	if (!Carry.IsHandEmpty())
	{
		OutFailureReason = LOCTEXT("HandOccupied", "이미 다른 물건을 들고 있습니다.");
		return false;
	}
	return ValidatePlacementPayload(OutFailureReason);
}

bool APlaceableFacilityItemActor::HandleTakenBy(
	UPlayerCarryComponent& Carry,
	USceneComponent* HeldAnchor)
{
	FText FailureReason;
	if (Lifecycle != ELifecycle::FreeWorld || Carrier.IsValid() || !HeldAnchor
		|| !ValidatePlacementPayload(FailureReason))
	{
		return false;
	}
	LastSafeTransform = GetActorTransform();
	Carrier = &Carry;
	Lifecycle = ELifecycle::Held;
	SetFreeWorldPhysics(false);
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (!AttachToComponent(HeldAnchor, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
	{
		Carrier.Reset();
		Lifecycle = ELifecycle::FreeWorld;
		return false;
	}
	const FTransform LocalHeld = GetHeldTransform();
	ItemRoot->SetRelativeLocationAndRotation(LocalHeld.GetLocation(), LocalHeld.GetRotation());
	return true;
}

bool APlaceableFacilityItemActor::CanFreeDrop(FText& OutFailureReason) const
{
	if (Lifecycle != ELifecycle::Held || !Carrier.IsValid()
		|| Carrier->GetHeldObject() != this)
	{
		OutFailureReason = LOCTEXT("FacilityItemNotHeld", "설비 아이템을 들고 있어야 내려놓을 수 있습니다.");
		return false;
	}
	return true;
}

UPrimitiveComponent* APlaceableFacilityItemActor::GetPhysicalCarryPrimitive() const
{
	return ItemRoot;
}

bool APlaceableFacilityItemActor::NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry)
{
	if (Lifecycle != ELifecycle::Held || Carrier.Get() != &Carry)
	{
		return false;
	}
	Carrier.Reset();
	Lifecycle = ELifecycle::FreeWorld;
	LastSafeTransform = GetActorTransform();
	return true;
}

void APlaceableFacilityItemActor::PublishPhysicalCarryCommit(
	const EPhysicalCarryCommitTransition Transition)
{
	(void)Transition;
}

void APlaceableFacilityItemActor::RecoverPhysicalCarryable(
	UPlayerCarryComponent* PreviousCarry)
{
	if (Lifecycle == ELifecycle::PlacementConsumption
		|| Lifecycle == ELifecycle::PlacementConsumed
		|| Lifecycle == ELifecycle::Staged)
	{
		return;
	}
	UPlayerCarryComponent* LiveCarrier = Carrier.Get();
	if (PreviousCarry && LiveCarrier && LiveCarrier != PreviousCarry)
	{
		return;
	}
	if (LiveCarrier && LiveCarrier->GetHeldObject() == this)
	{
		// RecoverHeldPhysicalObject clears the authoritative held reference first,
		// then re-enters this method to perform the local free-world recovery.
		LiveCarrier->RecoverHeldPhysicalObject(this);
		return;
	}
	Carrier.Reset();
	Lifecycle = ELifecycle::FreeWorld;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocationAndRotation(
		LastSafeTransform.GetLocation(),
		LastSafeTransform.GetRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SetFreeWorldPhysics(true);
	ItemRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
	ItemRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}

bool APlaceableFacilityItemActor::InitializeStaged(
	UFacilityPlacementDefinition& InDefinition,
	FText& OutFailureReason)
{
	if (HasActorBegunPlay() || Lifecycle != ELifecycle::Staged
		|| !InDefinition.ValidateRuntime(OutFailureReason))
	{
		return false;
	}
	UStaticMesh* Mesh = ResolveRecoveryMesh(InDefinition);
	if (!Mesh || !ValidateRecoveryMesh(*Mesh, OutFailureReason))
	{
		return false;
	}
	Payload.Definition = &InDefinition;
	ItemRoot->SetStaticMesh(Mesh);
	SetFreeWorldPhysics(false);
	return true;
}

bool APlaceableFacilityItemActor::SetPlacementPayload(
	const FFacilityPlacementPayload& InPayload,
	FText& OutFailureReason)
{
	if (Lifecycle != ELifecycle::Staged || InPayload.Definition != Payload.Definition
		|| !InPayload.Validate(*this, OutFailureReason))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = LOCTEXT("PayloadDefinitionMismatch", "설비 아이템과 변환 데이터의 정의가 일치하지 않습니다.");
		}
		return false;
	}
	Payload = InPayload;
	return true;
}

bool APlaceableFacilityItemActor::ValidatePlacementPayload(FText& OutFailureReason) const
{
	return Payload.Validate(*this, OutFailureReason)
		&& Payload.Definition->ValidateRuntime(OutFailureReason);
}

bool APlaceableFacilityItemActor::ActivateFreeWorld(
	const FTransform& WorldTransform,
	FText& OutFailureReason)
{
	if (Lifecycle != ELifecycle::Staged || WorldTransform.ContainsNaN()
		|| !ValidatePlacementPayload(OutFailureReason))
	{
		return false;
	}
	SetActorLocationAndRotation(
		WorldTransform.GetLocation(),
		WorldTransform.GetRotation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	SetFreeWorldPhysics(true);
	if (!ItemRoot->IsSimulatingPhysics())
	{
		OutFailureReason = LOCTEXT("FacilityItemPhysicsFailed", "설비 아이템 물리를 활성화할 수 없습니다.");
		SetFreeWorldPhysics(false);
		return false;
	}
	ItemRoot->SetPhysicsLinearVelocity(FVector::ZeroVector);
	ItemRoot->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Lifecycle = ELifecycle::FreeWorld;
	LastSafeTransform = GetActorTransform();
	return true;
}

bool APlaceableFacilityItemActor::BeginPlacementConsumption(
	UPlayerCarryComponent& ExpectedCarry,
	FText& OutFailureReason)
{
	if (Lifecycle != ELifecycle::Held || Carrier.Get() != &ExpectedCarry
		|| ExpectedCarry.GetHeldObject() != this)
	{
		OutFailureReason = LOCTEXT("FacilityItemConsumptionInvalid", "설비 아이템 소지 상태가 변경되었습니다.");
		return false;
	}
	Lifecycle = ELifecycle::PlacementConsumption;
	return true;
}

void APlaceableFacilityItemActor::RollbackPlacementConsumption(
	UPlayerCarryComponent& ExpectedCarry)
{
	if (Lifecycle == ELifecycle::PlacementConsumption)
	{
		Carrier = &ExpectedCarry;
		Lifecycle = ELifecycle::Held;
	}
}

bool APlaceableFacilityItemActor::DestroyForPlacementConsumption()
{
	if (Lifecycle != ELifecycle::PlacementConsumption)
	{
		return false;
	}
	Lifecycle = ELifecycle::PlacementConsumed;
	if (!Destroy())
	{
		Lifecycle = ELifecycle::PlacementConsumption;
		return false;
	}
	return true;
}

bool APlaceableFacilityItemActor::IsHeldForPlacement() const
{
	return Lifecycle == ELifecycle::Held && Carrier.IsValid()
		&& Carrier->GetHeldObject() == this;
}

void APlaceableFacilityItemActor::SetFreeWorldPhysics(const bool bEnabled)
{
	if (!ItemRoot)
	{
		return;
	}
	ItemRoot->SetSimulatePhysics(false);
	ItemRoot->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	ItemRoot->SetUseCCD(true);
	ItemRoot->SetCollisionEnabled(
		bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	ItemRoot->SetSimulatePhysics(bEnabled);
}

#undef LOCTEXT_NAMESPACE
