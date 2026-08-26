#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PhysicalCarryFixedSlot.generated.h"

class AActor;
class UPlayerCarryComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnPhysicalCarrySlotOccupancyChanged,
	bool,
	bIsOccupied);

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPhysicalCarryFixedSlot : public UInterface
{
	GENERATED_BODY()
};

/** Native-only exact-item fixed-slot contract. Queries must not mutate state. */
class BATHHOUSESIM_API IPhysicalCarryFixedSlot
{
	GENERATED_BODY()

public:
	virtual AActor* GetAssignedPhysicalCarryItem() const = 0;
	virtual AActor* GetStoredPhysicalCarryItem() const = 0;
	virtual USceneComponent* GetPhysicalCarryItemAnchor() const = 0;
	virtual FText GetPhysicalCarrySlotDisplayName() const = 0;
	virtual bool IsPhysicalCarrySlotOperational(FText* OutFailureReason = nullptr) const = 0;
	virtual bool QueryTakePhysicalCarry(
		const UPlayerCarryComponent& Carry,
		FText& OutFailureReason) const = 0;
	virtual bool QueryStorePhysicalCarry(
		const UPlayerCarryComponent& Carry,
		const AActor& Item,
		FText& OutFailureReason) const = 0;

	// Transaction-only mutation. These methods do not broadcast presentation delegates.
	virtual bool ApplyPhysicalCarrySlotOccupancy(AActor& ExpectedItem, bool bOccupied) = 0;
	virtual void NotifyPhysicalCarrySlotOccupancyCommitted() = 0;
	virtual bool TryRecoverAssignedPhysicalCarryItem(AActor& ExpectedItem) = 0;
	virtual void NotifyAssignedPhysicalCarryItemEnding(AActor& ExpectedItem) = 0;
	virtual void DisablePhysicalCarrySlot(const FText& FailureReason) = 0;
};
