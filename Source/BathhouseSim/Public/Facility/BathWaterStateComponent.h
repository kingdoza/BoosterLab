#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BathWaterStateComponent.generated.h"

UENUM(BlueprintType)
enum class EBathWaterState : uint8
{
	Empty,
	Filling,
	Filled,
	Draining
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBathWaterStateChanged,
	EBathWaterState,
	PreviousState,
	EBathWaterState,
	NewState);

UCLASS(ClassGroup = (Bathhouse), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UBathWaterStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBathWaterStateComponent();

	UFUNCTION(BlueprintPure, Category = "Bath Water")
	EBathWaterState GetWaterState() const { return WaterState; }

	UFUNCTION(BlueprintPure, Category = "Bath Water")
	bool IsEmpty() const { return WaterState == EBathWaterState::Empty; }

	UFUNCTION(BlueprintCallable, Category = "Bath Water")
	bool SetWaterState(EBathWaterState NewState);

	UFUNCTION(BlueprintPure, Category = "Bath Water")
	float GetNormalizedAmount() const { return NormalizedAmount; }

	UFUNCTION(BlueprintCallable, Category = "Bath Water")
	void SetNormalizedAmount(float NewAmount);

	void ResetEmptyForPlacement();

	UPROPERTY(BlueprintAssignable, Category = "Bath Water|Presentation")
	FOnBathWaterStateChanged OnWaterStateChanged;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bath Water")
	EBathWaterState WaterState = EBathWaterState::Empty;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bath Water", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NormalizedAmount = 0.0f;
};
