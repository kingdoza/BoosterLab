// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FirstPersonMovementComponent.generated.h"

UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class BOOSTERLAB_API UFirstPersonMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UFirstPersonMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void StartSprinting();

	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void StopSprinting();

	UFUNCTION(BlueprintCallable, Category = "Sprint")
	void SwitchSprinting();

	UFUNCTION(BlueprintPure, Category = "Sprint")
	bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintPure, Category = "Sprint")
	bool IsForwardAccelerating() const;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Sprint")
	bool bIsSprinting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint", meta = (ClampMin = "0.0"))
	float WalkingSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint", meta = (ClampMin = "0.0"))
	float SprintingSpeed = 600.0f;
};
