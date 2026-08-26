#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Interaction/InteractionTypes.h"
#include "Interaction/PhysicalCarryable.h"
#include "PlayerCarryComponent.generated.h"

class ABathhouseKeyActor;
class IPhysicalCarryFixedSlot;
class UPrimitiveComponent;
class UPlayerEquipmentUseComponent;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeldKeyChanged, ABathhouseKeyActor*, HeldKey);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHeldObjectChanged, AActor*, HeldObject);

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UPlayerCarryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerCarryComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ConfigureHeldAnchor(USceneComponent* InHeldAnchor);
	void ConfigureEquipmentUse(UPlayerEquipmentUseComponent* InEquipmentUse);

	UFUNCTION(BlueprintPure, Category = "Carry")
	bool IsHandEmpty() const { return HeldObject == nullptr; }

	UFUNCTION(BlueprintPure, Category = "Carry")
	ABathhouseKeyActor* GetHeldKey() const;

	UFUNCTION(BlueprintPure, Category = "Carry")
	AActor* GetHeldObject() const { return HeldObject; }

	UFUNCTION(BlueprintPure, Category = "Carry")
	EPhysicalCarryKind GetHeldKind() const;

	UPROPERTY(BlueprintAssignable, Category = "Carry")
	FOnHeldKeyChanged OnHeldKeyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Carry")
	FOnHeldObjectChanged OnHeldObjectChanged;

	bool CommitTakeKey(ABathhouseKeyActor* Key);
	bool CommitReleaseKey(ABathhouseKeyActor* Key);
	bool TryTakePhysicalObject(AActor* Object, FText& OutFailureReason);
	bool CommitReleasePhysicalObject(AActor* Object);
	bool RecoverHeldPhysicalObject(AActor* Object);
	FPlayerInteractionResult TryTakeFromFixedSlot(AActor* SlotActor);
	FPlayerInteractionResult TryStoreHeldObjectInFixedSlot(AActor* SlotActor);
	FPlayerInteractionResult TryFreeDropHeldObject(const FVector& Direction);

	/** Compatibility wrapper for one migration cycle. ViewOrigin is intentionally ignored. */
	FPlayerInteractionResult TryReleaseHeldEquipment(const FVector& ViewOrigin, const FVector& ThrowDirection);
	void NotifyHeldActorEnding(AActor* Object);
	USceneComponent* GetHeldAnchor() const { return HeldAnchor; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Carry|Drop", meta = (DeprecatedProperty, DeprecationMessage = "Held-position free drop no longer performs a target-location sweep."))
	TEnumAsByte<ECollisionChannel> DropSweepChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "Carry|Drop", meta = (ClampMin = "0.0", DeprecatedProperty, DeprecationMessage = "Held-position free drop no longer resolves a clearance target."))
	float DropSweepClearance = 2.0f;

private:
	friend class FBathhousePhysicalCarryDropTest;

	bool CommitHeldObjectWithoutNotification(AActor* Object);
	bool ClearHeldObjectWithoutNotification(AActor* ExpectedObject, bool& bOutWasKey);
	void PublishHeldObjectChange(AActor* NewHeldObject, bool bPreviousObjectWasKey);
	void PublishPhysicalCarryCommit(
		AActor* Object,
		AActor* SlotActor,
		EPhysicalCarryCommitTransition Transition,
		bool bNotifySlot,
		bool bPreviousObjectWasKey);
	bool IsHeldPoseClear(
		AActor& Object,
		UPrimitiveComponent& Primitive,
		FText& OutFailureReason) const;
	void CancelEquipmentUseForPlacement();

	UPROPERTY(Transient)
	TObjectPtr<AActor> HeldObject = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> HeldAnchor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerEquipmentUseComponent> EquipmentUseComponent = nullptr;

	bool bPhysicalDropCommitInProgress = false;
};
