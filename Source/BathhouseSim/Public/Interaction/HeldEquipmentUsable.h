#pragma once

#include "CoreMinimal.h"
#include "Interaction/InteractionTypes.h"
#include "UObject/Interface.h"
#include "HeldEquipmentUsable.generated.h"

class AActor;
class UCameraComponent;
class UHeldEquipmentMotionComponent;
class UPlayerCarryComponent;

USTRUCT()
struct BATHHOUSESIM_API FHeldEquipmentUseContext
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<AActor> User = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> Equipment = nullptr;

	UPROPERTY()
	TObjectPtr<UPlayerCarryComponent> CarryComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera = nullptr;

	UPROPERTY()
	TObjectPtr<UHeldEquipmentMotionComponent> MotionComponent = nullptr;

	UPROPERTY()
	FHitResult FocusHit;

	FVector CameraOrigin = FVector::ZeroVector;
	FVector CameraDirection = FVector::ForwardVector;
};

USTRUCT()
struct BATHHOUSESIM_API FHeldEquipmentUseQuery
{
	GENERATED_BODY()

	bool bVisible = true;
	bool bCanUse = false;
	FText DisplayName;
	FText ActionName;
	FText FailureReason;
	EPlayerInteractionActivationMode ActivationMode = EPlayerInteractionActivationMode::Instant;
	float Progress = 0.0f;
};

USTRUCT()
struct BATHHOUSESIM_API FHeldEquipmentUseResult
{
	GENERATED_BODY()

	bool bSucceeded = false;
	FText FailureReason;

	static FHeldEquipmentUseResult Succeeded()
	{
		FHeldEquipmentUseResult Result;
		Result.bSucceeded = true;
		return Result;
	}

	static FHeldEquipmentUseResult Failed(const FText& Reason)
	{
		FHeldEquipmentUseResult Result;
		Result.FailureReason = Reason;
		return Result;
	}
};

USTRUCT()
struct BATHHOUSESIM_API FHeldEquipmentUseUpdate
{
	GENERATED_BODY()

	EPlayerHoldInteractionState State = EPlayerHoldInteractionState::Failed;
	float Progress = 0.0f;
	FText FailureReason;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UHeldEquipmentUsable : public UInterface
{
	GENERATED_BODY()
};

class BATHHOUSESIM_API IHeldEquipmentUsable
{
	GENERATED_BODY()

public:
	virtual FHeldEquipmentUseQuery QueryEquipmentUse(const FHeldEquipmentUseContext& Context) const = 0;
	virtual FHeldEquipmentUseResult BeginEquipmentUse(const FHeldEquipmentUseContext& Context) = 0;
	virtual FHeldEquipmentUseUpdate UpdateEquipmentUse(const FHeldEquipmentUseContext& Context, float DeltaTime) = 0;
	virtual FHeldEquipmentUseResult EndEquipmentUse(const FHeldEquipmentUseContext& Context) = 0;
	virtual void CancelEquipmentUse(const FHeldEquipmentUseContext& Context) = 0;
};
