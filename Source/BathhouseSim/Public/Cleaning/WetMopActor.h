#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerInteractable.h"
#include "WetMopActor.generated.h"

class UPlayerCarryComponent;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWetMopHeldPresentationChanged, bool, bIsHeld);

UCLASS(Blueprintable)
class BATHHOUSESIM_API AWetMopActor : public AActor, public IPlayerInteractable, public IPhysicalCarryable
{
	GENERATED_BODY()

public:
	AWetMopActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual EPhysicalCarryKind GetPhysicalCarryKind() const override { return EPhysicalCarryKind::WetMop; }
	virtual FText GetPhysicalCarryDisplayName() const override;
	virtual bool CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const override;
	virtual bool HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor) override;
	virtual bool CanFreeDrop(FText& OutFailureReason) const override { return true; }
	virtual UPrimitiveComponent* GetPhysicalCarryPrimitive() const override;
	virtual float GetThrowSpawnDistance() const override { return ThrowSpawnDistance; }
	virtual float GetThrowImpulseStrength() const override { return ThrowImpulseStrength; }
	virtual void NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) override;
	virtual void RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry) override;

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnWetMopHeldPresentationChanged OnHeldPresentationChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cleaning")
	TObjectPtr<UStaticMeshComponent> WorldMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning|Carry", meta = (ClampMin = "0.0"))
	float ThrowImpulseStrength = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning|Carry", meta = (ClampMin = "0.0"))
	float ThrowSpawnDistance = 70.0f;

private:
	void SetWorldPhysics(bool bEnabled);

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> Carrier = nullptr;

	FTransform InitialTransform;
	FTransform LastSafeTransform;
};
