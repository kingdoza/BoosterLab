#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PhysicalCarryable.generated.h"

class UPlayerCarryComponent;
class UPrimitiveComponent;
class USceneComponent;
class AActor;

UENUM(BlueprintType)
enum class EPhysicalCarryKind : uint8
{
	None,
	Key,
	WetMop,
	TowelBasket,
	MonkeyWrench
};

UENUM(BlueprintType, meta = (Bitflags))
enum class EPhysicalCarryCapability : uint8
{
	None = 0,
	FreeDrop = 1 << 0,
	FixedSlot = 1 << 1
};
ENUM_CLASS_FLAGS(EPhysicalCarryCapability);

/** Native-only publication token used after a physical-carry transition can no longer roll back. */
enum class EPhysicalCarryCommitTransition : uint8
{
	TakenIntoHand,
	StoredInFixedSlot,
	DroppedToWorld,
	Recovered
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPhysicalCarryable : public UInterface
{
	GENERATED_BODY()
};

class BATHHOUSESIM_API IPhysicalCarryable
{
	GENERATED_BODY()

public:
	virtual EPhysicalCarryKind GetPhysicalCarryKind() const = 0;
	virtual FText GetPhysicalCarryDisplayName() const = 0;
	virtual EPhysicalCarryCapability GetPhysicalCarryCapabilities() const
	{
		return EPhysicalCarryCapability::FreeDrop | EPhysicalCarryCapability::FixedSlot;
	}
	virtual FTransform GetHeldTransform() const { return FTransform::Identity; }
	virtual bool CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const = 0;
	virtual bool HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor) = 0;
	virtual bool CanFreeDrop(FText& OutFailureReason) const
	{
		return EnumHasAnyFlags(GetPhysicalCarryCapabilities(), EPhysicalCarryCapability::FreeDrop);
	}
	virtual UPrimitiveComponent* GetPhysicalCarryPrimitive() const { return nullptr; }
	virtual float GetThrowSpawnDistance() const { return 0.0f; }
	virtual float GetThrowImpulseStrength() const { return 120.0f; }
	virtual float GetUpwardThrowImpulseStrength() const { return 15.0f; }

	virtual AActor* GetAssignedPhysicalCarryFixedSlot() const { return nullptr; }
	virtual bool TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason) { return false; }
	virtual void ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot) {}
	virtual void NotifyPhysicalCarryFixedSlotBindingConflict() {}
	virtual bool IsStoredInAssignedPhysicalCarryFixedSlot() const { return false; }
	// Commit preparation mutates concrete domain state but must not invoke external delegates.
	// A false return must leave concrete domain state unchanged.
	virtual bool NotifyTakenFromFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) { return false; }
	virtual bool NotifyStoredInFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) { return false; }
	virtual bool NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor) { return false; }
	virtual void NotifyFixedSlotDestroyed(AActor& SlotActor) {}
	virtual bool NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) { return false; }
	virtual void PublishPhysicalCarryCommit(EPhysicalCarryCommitTransition Transition) = 0;
	virtual void RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry) = 0;
};
