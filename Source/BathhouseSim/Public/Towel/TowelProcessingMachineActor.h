#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerInteractable.h"
#include "Interaction/SupplementalInteractionIntentSource.h"
#include "Placement/PlaceableFacility.h"
#include "Towel/TowelTypes.h"
#include "TowelProcessingMachineActor.generated.h"

class USceneComponent;
class UBoxComponent;
class UFacilityPlacementComponent;
class UPlayerCarryComponent;
class UTowelInventoryComponent;
class UTowelMachineControlComponent;
class UTowelPileVisualComponent;
class UTowelTransferPortComponent;
class UTowelTransferSubsystem;
class APlaceableFacilityItemActor;
struct FFacilityPlacementPayload;
struct FFacilityPlacementPublication;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTowelMachineStateChanged,
	ETowelMachineState,
	PreviousState,
	ETowelMachineState,
	NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTowelMachineProgressChanged, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnTowelMachineContentsChanged,
	const FTowelInventorySnapshot&,
	Snapshot);

UCLASS(Blueprintable)
class BATHHOUSESIM_API ATowelProcessingMachineActor
	: public AActor
	, public IPlayerInteractable
	, public ISupplementalInteractionIntentSource
	, public IPlaceableFacility
	, public IPhysicalCarryable
{
	GENERATED_BODY()

public:
	ATowelProcessingMachineActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;
	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual FPlayerInteractionQuery MergeSupplementalInteractionQuery(const FPlayerInteractionQuery& BaseQuery) const override;
	virtual UFacilityPlacementComponent* GetFacilityPlacementComponent() const override { return FacilityPlacement; }
	virtual FFacilityPlacementTransactionResult QueryFacilityPlacement(const FTransform& CandidateTransform, const class AFacilityPlacementZoneActor& Zone) const override;
	virtual FFacilityPlacementTransactionResult QueryFacilityRecovery() const override;
	virtual bool ExportPlacementPayload(APlaceableFacilityItemActor& Item, FFacilityPlacementPayload& OutPayload, FText& OutFailureReason) const override;
	virtual bool ImportPlacementPayload(const APlaceableFacilityItemActor& Item, const FFacilityPlacementPayload& Payload, FText& OutFailureReason) override;
	virtual bool StagePlacedDomainRegistration(FText& OutFailureReason) override;
	virtual void RollbackPlacedDomainRegistration() override;
	virtual bool StagePlacedDomainUnregistration(FFacilityPlacementPublication& OutPublication, FText& OutFailureReason) override;
	virtual bool RollbackPlacedDomainUnregistration(FText& OutFailureReason) override;
	virtual void PublishPlacedDomainRegistration() override;
	virtual bool CommitPlaceableFacilityMode(EPlaceableFacilityMode NewMode, FText& OutFailureReason) override;
	virtual EPhysicalCarryKind GetPhysicalCarryKind() const override { return EPhysicalCarryKind::Facility; }
	virtual EPhysicalCarryCapability GetPhysicalCarryCapabilities() const override { return EPhysicalCarryCapability::None; }
	virtual FText GetPhysicalCarryDisplayName() const override;
	virtual FTransform GetHeldTransform() const override;
	virtual bool CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const override;
	virtual bool HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor) override;
	virtual bool CanFreeDrop(FText& OutFailureReason) const override;
	virtual UPrimitiveComponent* GetPhysicalCarryPrimitive() const override;
	virtual float GetThrowImpulseStrength() const override;
	virtual float GetUpwardThrowImpulseStrength() const override;
	virtual AActor* GetAssignedPhysicalCarryFixedSlot() const override;
	virtual bool TryBindPhysicalCarryFixedSlot(AActor& SlotActor, FText& OutFailureReason) override;
	virtual void ClearPhysicalCarryFixedSlotBinding(AActor& ExpectedSlot) override;
	virtual void NotifyPhysicalCarryFixedSlotBindingConflict() override;
	virtual bool IsStoredInAssignedPhysicalCarryFixedSlot() const override;
	virtual bool NotifyTakenFromFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) override;
	virtual bool NotifyStoredInFixedSlotCommitted(UPlayerCarryComponent& Carry, AActor& SlotActor) override;
	virtual bool NotifyRecoveredToFixedSlotCommitted(AActor& SlotActor) override;
	virtual void NotifyFixedSlotDestroyed(AActor& SlotActor) override;
	virtual bool NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) override;
	virtual void PublishPhysicalCarryCommit(EPhysicalCarryCommitTransition Transition) override;
	virtual void RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry) override;

	UFUNCTION(BlueprintPure, Category = "Towel Machine")
	ETowelMachineState GetMachineState() const { return MachineState; }

	UFUNCTION(BlueprintPure, Category = "Towel Machine")
	ETowelMachineKind GetMachineKind() const { return MachineKind; }

	UFUNCTION(BlueprintPure, Category = "Towel Machine")
	float GetProcessingProgress() const;

	UFUNCTION(BlueprintPure, Category = "Towel Machine")
	UTowelInventoryComponent* GetInventory() const { return Inventory; }

	ETowelState GetInputState() const;
	ETowelState GetOutputState() const;
	bool CanStartProcessing(FText& OutFailureReason) const;
	bool StartProcessing(FText& OutFailureReason);

	UPROPERTY(BlueprintAssignable, Category = "Towel Machine|Presentation")
	FOnTowelMachineStateChanged OnMachineStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Towel Machine|Presentation")
	FOnTowelMachineProgressChanged OnMachineProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Towel Machine|Presentation")
	FOnTowelMachineContentsChanged OnMachineContentsChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel Machine")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	TObjectPtr<UBoxComponent> PackagePhysicalRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	TObjectPtr<UBoxComponent> PlacementFootprint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	TObjectPtr<UFacilityPlacementComponent> FacilityPlacement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel Machine")
	TObjectPtr<UTowelInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel Machine")
	TObjectPtr<UTowelTransferPortComponent> TransferPort;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel Machine")
	TObjectPtr<UTowelMachineControlComponent> MachineControl;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel Machine|Presentation")
	TObjectPtr<UTowelPileVisualComponent> TowelPresentationVisual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel Machine")
	ETowelMachineKind MachineKind = ETowelMachineKind::Washer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel Machine", meta = (ClampMin = "0.1"))
	float ProcessingDurationSeconds = 10.0f;

private:
	friend class FBathhouseFacilityPlacementRuntimeTest;
	friend class FBathhouseTowelTransferTest;
	friend class UTowelTransferSubsystem;

	bool AllowsInventoryTransfer(
		const UTowelInventoryComponent* Source,
		const UTowelInventoryComponent* Destination,
		const FTowelInventorySnapshot& SourceSnapshot,
		const FTowelInventorySnapshot& DestinationSnapshot) const;
	void HandleCommittedInventoryTransfer(const UTowelInventoryComponent* Source);

	UFUNCTION()
	void HandleInventoryChanged(
		const FTowelInventorySnapshot& Previous,
		const FTowelInventorySnapshot& Current,
		int64 TransactionId);

	void CompleteProcessing();
	void CommitMachineState(ETowelMachineState NewState);

	ETowelMachineState MachineState = ETowelMachineState::Waiting;
	double ProcessingEndTime = 0.0;
	FTimerHandle ProcessingTimerHandle;
};
