#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/HeldEquipmentUsable.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerInteractable.h"
#include "MonkeyWrenchActor.generated.h"

class UMeleeAttackComponent;
class UPlayerCarryComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMonkeyWrenchHeldPresentationChanged, bool, bIsHeld);

UCLASS(Blueprintable)
class BATHHOUSESIM_API AMonkeyWrenchActor : public AActor, public IPlayerInteractable, public IPhysicalCarryable, public IHeldEquipmentUsable
{
	GENERATED_BODY()

public:
	AMonkeyWrenchActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual EPhysicalCarryKind GetPhysicalCarryKind() const override { return EPhysicalCarryKind::MonkeyWrench; }
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

	virtual FHeldEquipmentUseQuery QueryEquipmentUse(const FHeldEquipmentUseContext& Context) const override;
	virtual FHeldEquipmentUseResult BeginEquipmentUse(const FHeldEquipmentUseContext& Context) override;
	virtual FHeldEquipmentUseUpdate UpdateEquipmentUse(const FHeldEquipmentUseContext& Context, float DeltaTime) override;
	virtual FHeldEquipmentUseResult EndEquipmentUse(const FHeldEquipmentUseContext& Context) override;
	virtual void CancelEquipmentUse(const FHeldEquipmentUseContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(BlueprintAssignable, Category = "Combat|Presentation")
	FOnMonkeyWrenchHeldPresentationChanged OnHeldPresentationChanged;

	UFUNCTION(BlueprintPure, Category = "Combat")
	UMeleeAttackComponent* GetMeleeAttack() const { return MeleeAttack; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UStaticMeshComponent> WorldMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UMeleeAttackComponent> MeleeAttack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Carry", meta = (ClampMin = "0.0"))
	float ThrowImpulseStrength = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Carry", meta = (ClampMin = "0.0"))
	float ThrowSpawnDistance = 75.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Carry|Presentation")
	FTransform HeldTransform = FTransform::Identity;

private:
	void ApplyHeldTransform();
	void SetWorldPhysics(bool bEnabled);

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> Carrier = nullptr;

	FTransform InitialTransform;
	FTransform LastSafeTransform;
};
