#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#include "Interaction/PlayerInteractable.h"
#include "PhysicalCarryFixedSlotActor.generated.h"

class UBoxComponent;
class USceneComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API APhysicalCarryFixedSlotActor
	: public AActor
	, public IPlayerInteractable
	, public IPhysicalCarryFixedSlot
{
	GENERATED_BODY()

public:
	APhysicalCarryFixedSlotActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;

	virtual AActor* GetAssignedPhysicalCarryItem() const override;
	virtual AActor* GetStoredPhysicalCarryItem() const override;
	virtual USceneComponent* GetPhysicalCarryItemAnchor() const override { return ItemAnchor; }
	virtual FText GetPhysicalCarrySlotDisplayName() const override { return SlotDisplayName; }
	virtual bool IsPhysicalCarrySlotOperational(FText* OutFailureReason = nullptr) const override;
	virtual bool QueryTakePhysicalCarry(
		const UPlayerCarryComponent& Carry,
		FText& OutFailureReason) const override;
	virtual bool QueryStorePhysicalCarry(
		const UPlayerCarryComponent& Carry,
		const AActor& Item,
		FText& OutFailureReason) const override;
	virtual bool ApplyPhysicalCarrySlotOccupancy(AActor& ExpectedItem, bool bOccupied) override;
	virtual void NotifyPhysicalCarrySlotOccupancyCommitted() override;
	virtual bool TryRecoverAssignedPhysicalCarryItem(AActor& ExpectedItem) override;
	virtual void NotifyAssignedPhysicalCarryItemEnding(AActor& ExpectedItem) override;
	virtual void DisablePhysicalCarrySlot(const FText& FailureReason) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintPure, Category = "Physical Carry Slot")
	bool IsOccupied() const { return bOccupied; }

	UPROPERTY(BlueprintAssignable, Category = "Physical Carry Slot|Presentation")
	FOnPhysicalCarrySlotOccupancyChanged OnSlotOccupancyChanged;

protected:
	friend class FBathhousePhysicalCarryFixedSlotTest;
	friend class FBathhousePhysicalCarryAtomicCommitTest;
	friend class FBathhousePhysicalCarryFallRecoveryTest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physical Carry Slot")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physical Carry Slot")
	TObjectPtr<UBoxComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Physical Carry Slot")
	TObjectPtr<USceneComponent> ItemAnchor;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Physical Carry Slot")
	TObjectPtr<AActor> AssignedItem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical Carry Slot")
	FText SlotDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical Carry Slot")
	bool bStartOccupied = true;

private:
	bool InitializeRuntimeSlot();
	bool ValidateAssignment(FText& OutFailureReason) const;
	void ReleaseStoredItemForSlotEndPlay();

	UPROPERTY(Transient)
	TObjectPtr<AActor> RuntimeAssignedItem = nullptr;

	FText RuntimeFailureReason;
	bool bOccupied = false;
	bool bRuntimeInitialized = false;
	bool bRuntimeOperational = false;
	bool bEndingPlay = false;
};
