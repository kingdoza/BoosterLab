#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/HeldEquipmentUsable.h"
#include "PlayerEquipmentUseComponent.generated.h"

class UCameraComponent;
class UHeldEquipmentMotionComponent;
class UPlayerCarryComponent;
class UPlayerInteractionComponent;

UCLASS(ClassGroup = (Interaction), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UPlayerEquipmentUseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerEquipmentUseComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Configure(
		UCameraComponent* InCamera,
		UPlayerCarryComponent* InCarry,
		UPlayerInteractionComponent* InInteraction,
		UHeldEquipmentMotionComponent* InMotion);

	FPlayerInteractionQuery MergeEquipmentQuery(const FPlayerInteractionQuery& BaseQuery) const;
	FPlayerInteractionResult BeginEquipmentUse();
	void UpdateEquipmentUse(float DeltaTime);
	void EndEquipmentUse();
	void CancelEquipmentUse();

	bool IsEquipmentUseInputActive() const { return bInputActive; }

private:
	friend class FBathhouseEquipmentUseRoutingTest;

	bool BuildContext(AActor* Equipment, FHeldEquipmentUseContext& OutContext) const;
	IHeldEquipmentUsable* GetHeldUsable(AActor*& OutEquipment) const;
	void ClearActiveUse();

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> Camera = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerCarryComponent> CarryComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerInteractionComponent> InteractionComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UHeldEquipmentMotionComponent> MotionComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ActiveEquipment = nullptr;

	UPROPERTY(Transient)
	FHeldEquipmentUseContext ActiveContext;
	EPlayerInteractionActivationMode ActiveMode = EPlayerInteractionActivationMode::Instant;
	bool bInputActive = false;
};
