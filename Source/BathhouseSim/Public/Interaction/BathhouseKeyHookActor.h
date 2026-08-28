#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PhysicalCarryFixedSlot.h"
#include "Interaction/PlayerInteractable.h"
#include "BathhouseKeyHookActor.generated.h"

class ABathhouseKeyActor;
class UBoxComponent;
class USceneComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseKeyHookActor
	: public AActor
	, public IPlayerInteractable
	, public IPhysicalCarryFixedSlot
{
	GENERATED_BODY()

public:
	ABathhouseKeyHookActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual AActor* GetAssignedPhysicalCarryItem() const override;
	virtual AActor* GetStoredPhysicalCarryItem() const override;
	virtual USceneComponent* GetPhysicalCarryItemAnchor() const override { return KeyAnchor; }
	virtual FText GetPhysicalCarrySlotDisplayName() const override;
	virtual bool IsPhysicalCarrySlotOperational(FText* OutFailureReason = nullptr) const override;
	virtual bool QueryTakePhysicalCarry(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const override;
	virtual bool QueryStorePhysicalCarry(const UPlayerCarryComponent& Carry, const AActor& Item, FText& OutFailureReason) const override;
	virtual bool ApplyPhysicalCarrySlotOccupancy(AActor& ExpectedItem, bool bOccupied) override;
	virtual void NotifyPhysicalCarrySlotOccupancyCommitted() override;
	virtual bool TryRecoverAssignedPhysicalCarryItem(AActor& ExpectedItem) override;
	virtual void NotifyAssignedPhysicalCarryItemEnding(AActor& ExpectedItem) override;
	virtual void DisablePhysicalCarrySlot(const FText& FailureReason) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintPure, Category = "Bathhouse Key")
	int32 GetKeyNumber() const { return KeyNumber; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Key")
	ABathhouseKeyActor* GetKeyActor() const { return KeyActor; }

	USceneComponent* GetKeyAnchor() const { return KeyAnchor; }
	bool IsNumberTopologyValid(FText* OutFailureReason = nullptr) const;

	UPROPERTY(BlueprintAssignable, Category = "Bathhouse Key|Presentation")
	FOnPhysicalCarrySlotOccupancyChanged OnSlotOccupancyChanged;

protected:
	friend class ABathhouseKeyActor;
	friend class FBathhouseKeyTopologyInitializationTest;
	friend class FBathhousePhysicalCarryDropTest;
	friend class FBathhousePhysicalCarryFixedSlotTest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<UBoxComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<USceneComponent> KeyAnchor;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Key", meta = (ClampMin = "0"))
	int32 KeyNumber = 0;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Key")
	TObjectPtr<ABathhouseKeyActor> KeyActor = nullptr;

private:
	bool InitializeRuntimeFixedSlot();
	void HandleKeyTopologyChanged();
	void ReleaseStoredKeyForEndPlay();

	FDelegateHandle KeyTopologyChangedHandle;

	FText RuntimeFailureReason;
	bool bSlotOccupied = false;
	bool bRuntimeOperational = false;
	bool bEndingPlay = false;
};
