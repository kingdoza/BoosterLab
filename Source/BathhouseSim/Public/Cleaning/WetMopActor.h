#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/HeldEquipmentUsable.h"
#include "Interaction/PlayerInteractable.h"
#include "WetMopActor.generated.h"

class UPlayerCarryComponent;
class USceneComponent;
class UStaticMeshComponent;
class UCurveVector;
class AWaterStainActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWetMopHeldPresentationChanged, bool, bIsHeld);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoppingStateChanged, bool, bIsMopping);

UCLASS(Blueprintable)
class BATHHOUSESIM_API AWetMopActor : public AActor, public IPlayerInteractable, public IPhysicalCarryable, public IHeldEquipmentUsable
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
	virtual FTransform GetHeldTransform() const override;
	virtual bool CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const override;
	virtual bool HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor) override;
	virtual bool CanFreeDrop(FText& OutFailureReason) const override;
	virtual UPrimitiveComponent* GetPhysicalCarryPrimitive() const override;
	virtual float GetThrowSpawnDistance() const override { return ThrowSpawnDistance; }
	virtual float GetThrowImpulseStrength() const override { return ThrowImpulseStrength; }
	virtual float GetUpwardThrowImpulseStrength() const override { return UpwardThrowImpulseStrength; }
	virtual AActor* GetAssignedPhysicalCarryFixedSlot() const override { return FixedSlot.Get(); }
	virtual bool TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason) override;
	virtual void ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot) override;
	virtual void NotifyPhysicalCarryFixedSlotBindingConflict() override { bFixedSlotBindingConflict = true; }
	virtual bool IsStoredInAssignedPhysicalCarryFixedSlot() const override;
	virtual bool NotifyTakenFromFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) override;
	virtual bool NotifyStoredInFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) override;
	virtual bool NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor) override;
	virtual void NotifyFixedSlotDestroyed(AActor& SlotActor) override;
	virtual bool NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) override;
	virtual void PublishPhysicalCarryCommit(EPhysicalCarryCommitTransition Transition) override;
	virtual void RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry) override;
	virtual FHeldEquipmentUseQuery QueryEquipmentUse(const FHeldEquipmentUseContext& Context) const override;
	virtual FHeldEquipmentUseResult BeginEquipmentUse(const FHeldEquipmentUseContext& Context) override;
	virtual FHeldEquipmentUseUpdate UpdateEquipmentUse(const FHeldEquipmentUseContext& Context, float DeltaTime) override;
	virtual FHeldEquipmentUseResult EndEquipmentUse(const FHeldEquipmentUseContext& Context) override;
	virtual void CancelEquipmentUse(const FHeldEquipmentUseContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnWetMopHeldPresentationChanged OnHeldPresentationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Cleaning|Presentation")
	FOnMoppingStateChanged OnMoppingStateChanged;

	UFUNCTION(BlueprintPure, Category = "Cleaning")
	bool IsMopping() const { return bIsMopping; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cleaning")
	TObjectPtr<UStaticMeshComponent> WorldMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning|Carry", meta = (ClampMin = "0.0"))
	float ThrowImpulseStrength = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning|Carry", meta = (ClampMin = "0.0", DeprecatedProperty, DeprecationMessage = "Held-position free drop no longer uses a camera-origin spawn distance."))
	float ThrowSpawnDistance = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleaning|Carry", meta = (ClampMin = "0.0"))
	float UpwardThrowImpulseStrength = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cleaning|Carry|Presentation")
	FTransform HeldTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cleaning|Mopping|Motion")
	TObjectPtr<UCurveVector> MoppingPositionCurve = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cleaning|Mopping|Motion")
	TObjectPtr<UCurveVector> MoppingRotationCurve = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cleaning|Mopping|Motion", meta = (ClampMin = "0.05"))
	float MoppingMotionPeriodSeconds = 0.8f;

private:
	friend class FBathhousePhysicalCarryDropTest;
	friend class FBathhousePhysicalCarryFixedSlotTest;
	friend class FBathhousePhysicalCarryAtomicCommitTest;
	friend class FBathhousePhysicalCarryFallRecoveryTest;
	friend class FBathhouseCleaningInteractionTest;

	void ApplyHeldTransform();
	void SetWorldPhysics(bool bEnabled);
	AWaterStainActor* ResolveFocusedStain(const FHeldEquipmentUseContext& Context) const;
	void ChangeActiveStain(AWaterStainActor* NewStain, AActor* Cleaner);
	void StopMopping(const FHeldEquipmentUseContext& Context);

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> Carrier = nullptr;

	TWeakObjectPtr<AActor> FixedSlot;

	UPROPERTY(Transient)
	TObjectPtr<AWaterStainActor> ActiveStain = nullptr;

	FTransform InitialTransform;
	FTransform LastSafeTransform;
	bool bIsMopping = false;
	bool bEndingPlay = false;
	bool bFixedSlotBindingConflict = false;
};
