#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "Facility/LockerCapacitySubsystem.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerInteractable.h"
#include "Interaction/SupplementalInteractionIntentSource.h"
#include "Placement/PlaceableFacility.h"
#include "BathhouseFacilityActor.generated.h"

class UBathhouseFacilitySlotComponent;
class UBathWaterStateComponent;
class UBoxComponent;
class UFacilityPlacementComponent;
class UPlayerCarryComponent;
class UPrimitiveComponent;
class USceneComponent;
class APlaceableFacilityItemActor;
struct FFacilityPlacementPayload;
struct FFacilityPlacementPublication;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseFacilityActor
	: public AActor
	, public IPlayerInteractable
	, public ISupplementalInteractionIntentSource
	, public IPlaceableFacility
	, public IPhysicalCarryable
{
	GENERATED_BODY()

public:
	ABathhouseFacilityActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;
#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostEditImport() override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

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

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	EBathhouseFacilityType GetFacilityType() const { return FacilityType; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	int32 GetFacilityNumber() const { return FacilityNumber; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	float GetSelectionWeight() const { return SelectionWeight; }

	UFUNCTION(BlueprintPure, Category = "Bathhouse Facility")
	bool IsFacilityEnabled() const { return bEnabled; }

	const TArray<TObjectPtr<UBathhouseFacilitySlotComponent>>& GetFacilitySlots() const { return FacilitySlots; }
	UBathWaterStateComponent* GetBathWaterState() const { return BathWaterState; }
	const FGuid& GetRegistrationId() const { return RegistrationId; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Facility")
	void OnSlotReservationChanged(UBathhouseFacilitySlotComponent* Slot, EBathhouseFacilitySlotState NewState);

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Facility")
	void OnSlotUseStarted(UBathhouseFacilitySlotComponent* Slot, AActor* User);

	UFUNCTION(BlueprintImplementableEvent, Category = "Bathhouse Facility")
	void OnSlotUseEnded(UBathhouseFacilitySlotComponent* Slot, AActor* User);

protected:
	friend class FBathhouseKeyTopologyInitializationTest;
	friend class FBathhouseFacilityPlacementRuntimeTest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	TObjectPtr<UBoxComponent> PackagePhysicalRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	TObjectPtr<UBoxComponent> PlacementFootprint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	TObjectPtr<UFacilityPlacementComponent> FacilityPlacement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility")
	TObjectPtr<UBathWaterStateComponent> BathWaterState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility")
	EBathhouseFacilityType FacilityType = EBathhouseFacilityType::Bath;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Facility")
	int32 FacilityNumber = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility", meta = (ClampMin = "0.0"))
	float SelectionWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Facility")
	bool bEnabled = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bathhouse Facility|Startup")
	FGuid RegistrationId;

private:
	friend class UBathhouseFacilitySubsystem;
	UFUNCTION()
	void HandleSlotStateChanged(UBathhouseFacilitySlotComponent* Slot, EBathhouseFacilitySlotState PreviousState, EBathhouseFacilitySlotState NewState);
	bool ValidatePlacedDomain(FText& OutFailureReason) const;
	bool RegisterPlacedDomain(FText& OutFailureReason, bool bPublish = true);
	ELockerBankRegistrationResult RegisterStartupLockerDomain(FText& OutFailureReason);
	bool CommitStartupLockerDomain(FText& OutFailureReason);
	void FailStartupLockerDomain();
	void UnregisterPlacedDomain(bool bUnexpectedEndPlay, bool bPublish = true);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBathhouseFacilitySlotComponent>> FacilitySlots;
	bool bPlacedDomainRegistered = false;
	bool bEndingPlay = false;
};
