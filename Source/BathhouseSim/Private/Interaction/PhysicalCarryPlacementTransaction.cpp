#include "Interaction/PhysicalCarryPlacementTransaction.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Interaction/PhysicalCarryFixedSlot.h"

FPhysicalCarryPlacementTransaction::FPhysicalCarryPlacementTransaction(
	AActor& InItem,
	UPrimitiveComponent& InPrimitive,
	AActor* InSlotActor)
	: Item(&InItem)
	, Primitive(&InPrimitive)
	, PreviousAttachParent(InPrimitive.GetAttachParent())
	, PreviousAttachSocket(InPrimitive.GetAttachSocketName())
	, PreviousRelativeTransform(InPrimitive.GetRelativeTransform())
	, PreviousWorldTransform(InItem.GetActorTransform())
	, PreviousCollisionEnabled(InPrimitive.GetCollisionEnabled())
	, PreviousCollisionResponses(InPrimitive.GetCollisionResponseToChannels())
	, PreviousObjectType(InPrimitive.GetCollisionObjectType())
	, PreviousLinearVelocity(InPrimitive.GetPhysicsLinearVelocity())
	, PreviousAngularVelocity(InPrimitive.GetPhysicsAngularVelocityInDegrees())
	, bPreviousSimulatePhysics(InPrimitive.IsSimulatingPhysics())
	, bPreviousEnableGravity(InPrimitive.IsGravityEnabled())
	, bPreviousUseCCD(InPrimitive.BodyInstance.bUseCCD)
	, SlotActor(InSlotActor)
{
	Slot = Cast<IPhysicalCarryFixedSlot>(InSlotActor);
	bPreviousSlotOccupied = Slot && Slot->GetStoredPhysicalCarryItem() == &InItem;
	bSnapshotValid = InItem.GetRootComponent() == &InPrimitive;
}

FPhysicalCarryPlacementTransaction::~FPhysicalCarryPlacementTransaction()
{
	if (!bCommitted)
	{
		Rollback();
	}
}

bool FPhysicalCarryPlacementTransaction::ApplyHeld(
	USceneComponent& HeldAnchor,
	const FTransform& HeldTransform)
{
	UPrimitiveComponent* RootPrimitive = Primitive.Get();
	if (!bSnapshotValid || !RootPrimitive)
	{
		return false;
	}

	RootPrimitive->SetSimulatePhysics(false);
	RootPrimitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootPrimitive->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	if (!RootPrimitive->AttachToComponent(
		&HeldAnchor,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale))
	{
		return false;
	}
	RootPrimitive->SetRelativeLocationAndRotation(HeldTransform.GetLocation(), HeldTransform.GetRotation());
	return RootPrimitive->GetAttachParent() == &HeldAnchor
		&& !RootPrimitive->IsSimulatingPhysics()
		&& RootPrimitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision;
}

bool FPhysicalCarryPlacementTransaction::ApplyFixedSlot(USceneComponent& ItemAnchor)
{
	UPrimitiveComponent* RootPrimitive = Primitive.Get();
	if (!bSnapshotValid || !RootPrimitive)
	{
		return false;
	}

	RootPrimitive->SetSimulatePhysics(false);
	RootPrimitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RootPrimitive->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	if (!RootPrimitive->AttachToComponent(
		&ItemAnchor,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale))
	{
		return false;
	}
	RootPrimitive->SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
	return RootPrimitive->GetAttachParent() == &ItemAnchor
		&& !RootPrimitive->IsSimulatingPhysics()
		&& RootPrimitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision;
}

bool FPhysicalCarryPlacementTransaction::ApplyFreeWorld(const FVector& VelocityChange)
{
	UPrimitiveComponent* RootPrimitive = Primitive.Get();
	if (!bSnapshotValid || !RootPrimitive || VelocityChange.ContainsNaN())
	{
		return false;
	}

	RootPrimitive->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	RootPrimitive->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	RootPrimitive->SetUseCCD(true);
	RootPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RootPrimitive->SetSimulatePhysics(true);
	if (!RootPrimitive->IsSimulatingPhysics())
	{
		return false;
	}

	RootPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
	RootPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	RootPrimitive->WakeAllRigidBodies();
	if (!VelocityChange.IsNearlyZero())
	{
		RootPrimitive->AddImpulse(VelocityChange, NAME_None, true);
	}
	return RootPrimitive->GetAttachParent() == nullptr
		&& RootPrimitive->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics
		&& RootPrimitive->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore
		&& RootPrimitive->BodyInstance.bUseCCD;
}

bool FPhysicalCarryPlacementTransaction::ApplySlotOccupancy(const bool bOccupied)
{
	AActor* CurrentItem = Item.Get();
	if (!Slot || !CurrentItem || !SlotActor.IsValid()
		|| !Slot->ApplyPhysicalCarrySlotOccupancy(*CurrentItem, bOccupied))
	{
		return false;
	}
	bSlotOccupancyChanged = bOccupied != bPreviousSlotOccupied;
	return true;
}

void FPhysicalCarryPlacementTransaction::Rollback()
{
	if (!bSnapshotValid)
	{
		return;
	}

	AActor* CurrentItem = Item.Get();
	UPrimitiveComponent* RootPrimitive = Primitive.Get();
	if (!CurrentItem || !RootPrimitive)
	{
		bSnapshotValid = false;
		return;
	}

	if (bSlotOccupancyChanged && Slot && SlotActor.IsValid())
	{
		Slot->ApplyPhysicalCarrySlotOccupancy(*CurrentItem, bPreviousSlotOccupied);
		bSlotOccupancyChanged = false;
	}

	RootPrimitive->SetSimulatePhysics(false);
	RootPrimitive->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	CurrentItem->SetActorTransform(
		PreviousWorldTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	if (USceneComponent* AttachParent = PreviousAttachParent.Get())
	{
		RootPrimitive->AttachToComponent(
			AttachParent,
			FAttachmentTransformRules::KeepWorldTransform,
			PreviousAttachSocket);
		RootPrimitive->SetRelativeTransform(PreviousRelativeTransform);
	}
	RootPrimitive->SetCollisionObjectType(PreviousObjectType);
	RootPrimitive->SetCollisionResponseToChannels(PreviousCollisionResponses);
	RootPrimitive->SetCollisionEnabled(PreviousCollisionEnabled);
	RootPrimitive->SetEnableGravity(bPreviousEnableGravity);
	RootPrimitive->SetUseCCD(bPreviousUseCCD);
	RootPrimitive->SetSimulatePhysics(bPreviousSimulatePhysics);
	if (bPreviousSimulatePhysics)
	{
		RootPrimitive->SetPhysicsLinearVelocity(PreviousLinearVelocity);
		RootPrimitive->SetPhysicsAngularVelocityInDegrees(PreviousAngularVelocity);
	}
	bSnapshotValid = false;
}
