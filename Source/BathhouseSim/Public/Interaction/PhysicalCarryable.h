#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PhysicalCarryable.generated.h"

class UPlayerCarryComponent;
class UPrimitiveComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EPhysicalCarryKind : uint8
{
	None,
	Key,
	WetMop,
	TowelBasket,
	MonkeyWrench
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
	virtual FTransform GetHeldTransform() const { return FTransform::Identity; }
	virtual bool CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const = 0;
	virtual bool HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor) = 0;
	virtual bool CanFreeDrop(FText& OutFailureReason) const { return false; }
	virtual UPrimitiveComponent* GetPhysicalCarryPrimitive() const { return nullptr; }
	virtual float GetThrowSpawnDistance() const { return 0.0f; }
	virtual float GetThrowImpulseStrength() const { return 0.0f; }
	virtual void NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) {}
	virtual void RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry) = 0;
};
