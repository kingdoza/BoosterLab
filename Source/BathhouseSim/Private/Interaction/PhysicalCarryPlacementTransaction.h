#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

class AActor;
class IPhysicalCarryFixedSlot;
class UPrimitiveComponent;
class USceneComponent;

/** Mechanical snapshot/rollback helper. Gameplay validation and final commits stay in the carry component. */
class FPhysicalCarryPlacementTransaction
{
public:
	FPhysicalCarryPlacementTransaction(
		AActor& InItem,
		UPrimitiveComponent& InPrimitive,
		AActor* InSlotActor = nullptr);
	~FPhysicalCarryPlacementTransaction();

	bool IsValid() const { return bSnapshotValid; }
	bool ApplyHeld(USceneComponent& HeldAnchor, const FTransform& HeldTransform);
	bool ApplyFixedSlot(USceneComponent& ItemAnchor);
	bool ApplyFreeWorld(const FVector& VelocityChange);
	bool ApplySlotOccupancy(bool bOccupied);
	void Commit() { bCommitted = true; }
	void Rollback();

private:
	TWeakObjectPtr<AActor> Item;
	TWeakObjectPtr<UPrimitiveComponent> Primitive;
	TWeakObjectPtr<USceneComponent> PreviousAttachParent;
	FName PreviousAttachSocket = NAME_None;
	FTransform PreviousRelativeTransform;
	FTransform PreviousWorldTransform;
	ECollisionEnabled::Type PreviousCollisionEnabled = ECollisionEnabled::NoCollision;
	FCollisionResponseContainer PreviousCollisionResponses;
	TEnumAsByte<ECollisionChannel> PreviousObjectType = ECC_WorldDynamic;
	FVector PreviousLinearVelocity = FVector::ZeroVector;
	FVector PreviousAngularVelocity = FVector::ZeroVector;
	bool bPreviousSimulatePhysics = false;
	bool bPreviousEnableGravity = true;
	bool bPreviousUseCCD = false;

	TWeakObjectPtr<AActor> SlotActor;
	IPhysicalCarryFixedSlot* Slot = nullptr;
	bool bPreviousSlotOccupied = false;
	bool bSlotOccupancyChanged = false;
	bool bSnapshotValid = false;
	bool bCommitted = false;
};
