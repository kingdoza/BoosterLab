// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FirstPersonCharacter.generated.h"

class UCameraComponent;
class UFirstPersonCameraShakeComponent;
class UFirstPersonMovementComponent;
class UInputAction;
struct FInputActionValue;

UCLASS()
class FIRSTPERSONTEMPLATE_API AFirstPersonCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AFirstPersonCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintPure, Category = "First Person")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "Movement")
	UFirstPersonMovementComponent* GetFirstPersonMovement() const { return FirstPersonMovement; }

	UFUNCTION(BlueprintPure, Category = "Camera Shake")
	UFirstPersonCameraShakeComponent* GetFirstPersonCameraShake() const { return FirstPersonCameraShake; }

protected:
	void MoveInput(const FInputActionValue& Value);
	void LookInput(const FInputActionValue& Value);
	void SprintStartInput();
	void SprintReleaseInput();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category = "Input")
	void DoJumpEnd();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "First Person", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFirstPersonMovementComponent> FirstPersonMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera Shake", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFirstPersonCameraShakeComponent> FirstPersonCameraShake;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveSpeedScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look", meta = (ClampMin = "0.0"))
	float LookSpeedScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sprint")
	bool bSprintToggle = true;
};
