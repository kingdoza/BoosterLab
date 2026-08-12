#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "InteractionTypes.generated.h"

class AActor;
class UActorComponent;
class UPlayerCarryComponent;

UENUM(BlueprintType)
enum class EPlayerInteractionIntent : uint8
{
	Primary,
	Secondary,
	DropCarry
};

UENUM(BlueprintType)
enum class EPlayerInteractionActivationMode : uint8
{
	Instant,
	Hold
};

UENUM()
enum class EPlayerHoldInteractionState : uint8
{
	Running,
	Succeeded,
	Failed
};

USTRUCT()
struct BATHHOUSESIM_API FPlayerHoldInteractionUpdate
{
	GENERATED_BODY()

	EPlayerHoldInteractionState State = EPlayerHoldInteractionState::Failed;
	float Progress = 0.0f;
	FText FailureReason;
};

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FPlayerInteractionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> Interactor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UPlayerCarryComponent> CarryComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> HitActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UActorComponent> HitComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FHitResult HitResult;
};

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FPlayerInteractionQuery
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bCanInteract = false;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText TargetName;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText ActionName;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	EPlayerInteractionActivationMode PrimaryActivationMode = EPlayerInteractionActivationMode::Instant;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HoldProgress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bSecondaryVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bCanSecondaryInteract = false;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText SecondaryActionName;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText SecondaryFailureReason;

	bool Equals(const FPlayerInteractionQuery& Other) const
	{
		return bVisible == Other.bVisible
			&& bCanInteract == Other.bCanInteract
			&& TargetName.EqualTo(Other.TargetName)
			&& ActionName.EqualTo(Other.ActionName)
			&& FailureReason.EqualTo(Other.FailureReason)
			&& PrimaryActivationMode == Other.PrimaryActivationMode
			&& FMath::IsNearlyEqual(HoldProgress, Other.HoldProgress)
			&& bSecondaryVisible == Other.bSecondaryVisible
			&& bCanSecondaryInteract == Other.bCanSecondaryInteract
			&& SecondaryActionName.EqualTo(Other.SecondaryActionName)
			&& SecondaryFailureReason.EqualTo(Other.SecondaryFailureReason);
	}
};

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FPlayerInteractionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	EPlayerInteractionIntent Intent = EPlayerInteractionIntent::Primary;

	static FPlayerInteractionResult Succeeded(
		const EPlayerInteractionIntent InIntent = EPlayerInteractionIntent::Primary)
	{
		FPlayerInteractionResult Result;
		Result.bSucceeded = true;
		Result.Intent = InIntent;
		return Result;
	}

	static FPlayerInteractionResult Failed(
		const FText& Reason,
		const EPlayerInteractionIntent InIntent = EPlayerInteractionIntent::Primary)
	{
		FPlayerInteractionResult Result;
		Result.FailureReason = Reason;
		Result.Intent = InIntent;
		return Result;
	}
};
