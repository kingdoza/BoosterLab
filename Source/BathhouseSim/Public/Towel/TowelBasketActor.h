#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerInteractable.h"
#include "TowelBasketActor.generated.h"

class UPlayerCarryComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTowelInventoryComponent;
class UTowelStackVisualComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTowelBasketHeldPresentationChanged, bool, bIsHeld);

UCLASS(Blueprintable)
class BATHHOUSESIM_API ATowelBasketActor : public AActor, public IPlayerInteractable, public IPhysicalCarryable
{
	GENERATED_BODY()

public:
	ATowelBasketActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual EPhysicalCarryKind GetPhysicalCarryKind() const override { return EPhysicalCarryKind::TowelBasket; }
	virtual FText GetPhysicalCarryDisplayName() const override;
	virtual FTransform GetHeldTransform() const override;
	virtual bool CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const override;
	virtual bool HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor) override;
	virtual bool CanFreeDrop(FText& OutFailureReason) const override { return true; }
	virtual UPrimitiveComponent* GetPhysicalCarryPrimitive() const override;
	virtual float GetThrowSpawnDistance() const override { return ThrowSpawnDistance; }
	virtual float GetThrowImpulseStrength() const override { return ThrowImpulseStrength; }
	virtual void NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) override;
	virtual void RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintPure, Category = "Towel")
	UTowelInventoryComponent* GetInventory() const { return Inventory; }

	UPROPERTY(BlueprintAssignable, Category = "Towel|Presentation")
	FOnTowelBasketHeldPresentationChanged OnHeldPresentationChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel")
	TObjectPtr<UStaticMeshComponent> WorldMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel")
	TObjectPtr<UTowelInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel|Presentation")
	TObjectPtr<UTowelStackVisualComponent> TowelPresentationVisual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Carry", meta = (ClampMin = "0.0"))
	float ThrowImpulseStrength = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Carry", meta = (ClampMin = "0.0"))
	float ThrowSpawnDistance = 70.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Towel|Carry|Presentation")
	FTransform HeldTransform = FTransform::Identity;

private:
	friend class FBathhousePhysicalCarryDropTest;

	void ApplyHeldTransform();
	void SetWorldPhysics(bool bEnabled);

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> Carrier = nullptr;

	FTransform InitialTransform;
	FTransform LastSafeTransform;
	bool bContentsRecovered = false;
};
