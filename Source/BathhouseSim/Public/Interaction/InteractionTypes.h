#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "InteractionTypes.generated.h"

class AActor;
class UActorComponent;
class UPlayerCarryComponent;

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

	bool Equals(const FPlayerInteractionQuery& Other) const
	{
		return bVisible == Other.bVisible
			&& bCanInteract == Other.bCanInteract
			&& TargetName.EqualTo(Other.TargetName)
			&& ActionName.EqualTo(Other.ActionName)
			&& FailureReason.EqualTo(Other.FailureReason);
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

	static FPlayerInteractionResult Succeeded()
	{
		FPlayerInteractionResult Result;
		Result.bSucceeded = true;
		return Result;
	}

	static FPlayerInteractionResult Failed(const FText& Reason)
	{
		FPlayerInteractionResult Result;
		Result.FailureReason = Reason;
		return Result;
	}
};
