#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Placement/FacilityPlacementTypes.h"
#include "FacilityPlacementComponent.generated.h"

class AFacilityPlacementZoneActor;
class UNavModifierComponent;
class UBoxComponent;
class UFacilityPlacementDefinition;
class UPlayerCarryComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnPlaceableFacilityModeChanged,
	EPlaceableFacilityMode,
	PreviousMode,
	EPlaceableFacilityMode,
	NewMode);

UCLASS(ClassGroup = (Placement), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UFacilityPlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFacilityPlacementComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Configure(
		UBoxComponent* InPlacementFootprint,
		UPrimitiveComponent* InPackagePhysicalRoot,
		UNavModifierComponent* InNavModifier);

	UFUNCTION(BlueprintPure, Category = "Facility Placement")
	EPlaceableFacilityMode GetMode() const { return Mode; }

	UFUNCTION(BlueprintPure, Category = "Facility Placement")
	UFacilityPlacementDefinition* GetDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure, Category = "Facility Placement")
	UBoxComponent* GetPlacementFootprint() const { return PlacementFootprint; }

	UPrimitiveComponent* GetPackagePhysicalRoot() const { return PackagePhysicalRoot; }
	bool IsOperational(FText& OutFailureReason) const;
	bool ValidateFootprintContractForDefinition(const UFacilityPlacementDefinition& InDefinition, FText& OutFailureReason) const;
	bool ValidateFootprintContract(FText& OutFailureReason) const;
	bool BuildPlacedActorTransform(const FTransform& RequestedTransform, FTransform& OutTransform, FText& OutFailureReason) const;
	bool GetFootprintRelativeToRoot(FTransform& OutTransform, FText& OutFailureReason) const;
	bool GetRecoveryDropTransform(FTransform& OutTransform, FText& OutFailureReason) const;
	bool CanEnablePackagedCollision(FText& OutFailureReason) const;
	bool BeginTransition(FText& OutFailureReason);
	void EndTransition();
	bool ApplyMode(EPlaceableFacilityMode NewMode, bool bFreeWorldPhysics, FText& OutFailureReason, bool bPublish = true);
	void PublishModeChanged(EPlaceableFacilityMode PreviousMode, EPlaceableFacilityMode NewMode);
	void ApplyHeldPresentation(class USceneComponent& HeldAnchor, const FTransform& HeldTransform);
	void RestoreLastSafePackagedWorld();
	void CaptureLastSafeTransform();
	void PrepareForStagedPlacement(UFacilityPlacementDefinition& InDefinition);
	void SetPlacedDomainActive(bool bActive);
	void CommitStagedPlacement();
	bool IsStagedPlacement() const { return bStagedPlacement; }
	bool IsPlacedDomainActive() const { return bPlacedDomainActive; }

	AActor* GetAssignedFixedSlot() const { return AssignedFixedSlot.Get(); }
	bool TryBindFixedSlot(AActor& SlotActor, FText& OutFailureReason);
	void ClearFixedSlot(AActor& ExpectedSlot);
	void MarkFixedSlotBindingConflict() { bFixedSlotBindingConflict = true; }
	bool IsStoredInFixedSlot() const;

	UPROPERTY(BlueprintAssignable, Category = "Facility Placement|Presentation")
	FOnPlaceableFacilityModeChanged OnModeChanged;

protected:
	friend class FBathhouseFacilityPlacementRuntimeTest;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facility Placement")
	TObjectPtr<UFacilityPlacementDefinition> Definition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	EPlaceableFacilityMode Mode = EPlaceableFacilityMode::Placed;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facility Placement|Carry")
	FTransform HeldTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facility Placement|Carry", meta = (ClampMin = "0.0"))
	float ThrowImpulseStrength = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facility Placement|Carry", meta = (ClampMin = "0.0"))
	float UpwardThrowImpulseStrength = 15.0f;

public:
	FTransform GetHeldTransform() const { return HeldTransform; }
	float GetThrowImpulseStrength() const { return ThrowImpulseStrength; }
	float GetUpwardThrowImpulseStrength() const { return UpwardThrowImpulseStrength; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> PlacementFootprint = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> PackagePhysicalRoot = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UNavModifierComponent> NavModifier = nullptr;

	TWeakObjectPtr<AActor> AssignedFixedSlot;
	FTransform LastSafeTransform = FTransform::Identity;
	bool bTransitionInProgress = false;
	bool bFixedSlotBindingConflict = false;
	bool bStagedPlacement = false;
	bool bPlacedDomainActive = false;
};
