#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PhysicalCarryable.h"
#include "Interaction/PlayerInteractable.h"
#include "Placement/FacilityPlacementPayload.h"
#include "PlaceableFacilityItemActor.generated.h"

class UFacilityPlacementDefinition;
class UPlayerCarryComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Blueprintable, NotPlaceable)
class BATHHOUSESIM_API APlaceableFacilityItemActor
	: public AActor
	, public IPlayerInteractable
	, public IPhysicalCarryable
{
	GENERATED_BODY()

public:
	APlaceableFacilityItemActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FellOutOfWorld(const UDamageType& DamageType) override;

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual EPhysicalCarryKind GetPhysicalCarryKind() const override { return EPhysicalCarryKind::Facility; }
	virtual FText GetPhysicalCarryDisplayName() const override;
	virtual EPhysicalCarryCapability GetPhysicalCarryCapabilities() const override
	{
		return EPhysicalCarryCapability::FreeDrop;
	}
	virtual FTransform GetHeldTransform() const override;
	virtual bool CanBeTakenBy(const UPlayerCarryComponent& Carry, FText& OutFailureReason) const override;
	virtual bool HandleTakenBy(UPlayerCarryComponent& Carry, USceneComponent* HeldAnchor) override;
	virtual bool CanFreeDrop(FText& OutFailureReason) const override;
	virtual UPrimitiveComponent* GetPhysicalCarryPrimitive() const override;
	virtual float GetThrowImpulseStrength() const override { return ThrowImpulseStrength; }
	virtual float GetUpwardThrowImpulseStrength() const override { return UpwardThrowImpulseStrength; }
	virtual bool NotifyPhysicalDropCommitted(UPlayerCarryComponent& Carry) override;
	virtual void PublishPhysicalCarryCommit(EPhysicalCarryCommitTransition Transition) override;
	virtual void RecoverPhysicalCarryable(UPlayerCarryComponent* PreviousCarry) override;

	bool InitializeStaged(UFacilityPlacementDefinition& InDefinition, FText& OutFailureReason);
	bool SetPlacementPayload(const FFacilityPlacementPayload& InPayload, FText& OutFailureReason);
	bool ValidatePlacementPayload(FText& OutFailureReason) const;
	bool ActivateFreeWorld(const FTransform& WorldTransform, FText& OutFailureReason);
	bool BeginPlacementConsumption(UPlayerCarryComponent& ExpectedCarry, FText& OutFailureReason);
	void RollbackPlacementConsumption(UPlayerCarryComponent& ExpectedCarry);
	bool DestroyForPlacementConsumption();

	UFacilityPlacementDefinition* GetDefinition() const { return Payload.Definition; }
	const FFacilityPlacementPayload& GetPlacementPayload() const { return Payload; }
	UStaticMeshComponent* GetItemRoot() const { return ItemRoot; }
	bool IsHeldForPlacement() const;

	static UStaticMesh* ResolveRecoveryMesh(const UFacilityPlacementDefinition& Definition);
	static bool ValidateRecoveryMesh(const UStaticMesh& Mesh, FText& OutFailureReason);
	static bool BuildDefinitionCollisionQuery(
		const UFacilityPlacementDefinition& Definition,
		const FTransform& ItemWorldTransform,
		FVector& OutLocation,
		FQuat& OutRotation,
		FCollisionShape& OutShape,
		const UPrimitiveComponent*& OutCollisionTemplate,
		FText& OutFailureReason);

protected:
	friend class FBathhouseFacilityPlacementRuntimeTest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Item")
	TObjectPtr<UStaticMeshComponent> ItemRoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facility Item|Carry")
	FTransform HeldTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facility Item|Carry", meta = (ClampMin = "0.0"))
	float ThrowImpulseStrength = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Facility Item|Carry", meta = (ClampMin = "0.0"))
	float UpwardThrowImpulseStrength = 15.0f;

private:
	enum class ELifecycle : uint8
	{
		Staged,
		FreeWorld,
		Held,
		PlacementConsumption,
		PlacementConsumed
	};

	void SetFreeWorldPhysics(bool bEnabled);

	UPROPERTY(Transient)
	FFacilityPlacementPayload Payload;

	TWeakObjectPtr<UPlayerCarryComponent> Carrier;
	FTransform LastSafeTransform = FTransform::Identity;
	ELifecycle Lifecycle = ELifecycle::Staged;
};
