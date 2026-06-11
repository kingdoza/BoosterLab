// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FirstPersonCameraShakeComponent.generated.h"

class AFirstPersonCharacter;
class APlayerCameraManager;
class APlayerController;
class UCameraShakeBase;
class UFirstPersonMovementComponent;

UENUM(BlueprintType)
enum class EFirstPersonCameraMoveState : uint8
{
	Idle,
	Walk,
	Sprint
};

UCLASS(ClassGroup = (Camera), meta = (BlueprintSpawnableComponent))
class BOOSTERLAB_API UFirstPersonCameraShakeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFirstPersonCameraShakeComponent();

	UFUNCTION(BlueprintCallable, Category = "Camera Shake")
	void StopAllCameraShakes();

	UFUNCTION(BlueprintCallable, Category = "Camera Shake")
	void SuppressNextLandingShake();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> IdleCameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> WalkCameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> SprintCameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Shake")
	TSubclassOf<UCameraShakeBase> LandingCameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera Shake", meta = (ClampMin = "0.0"))
	float IdleSpeedThreshold = 3.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	EFirstPersonCameraMoveState CalculateMoveState() const;
	void ApplyMoveState(EFirstPersonCameraMoveState NewState);
	void StopCurrentShake();
	void StopMoveShake();
	void PlayLandingShake();

	TSubclassOf<UCameraShakeBase> GetShakeClassForState(EFirstPersonCameraMoveState State) const;
	APlayerController* GetLocalPlayerController() const;
	bool ShouldDisableTickForNonLocal() const;
	bool IsOwnerFalling() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<AFirstPersonCharacter> OwnerCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UFirstPersonMovementComponent> OwnerMovementComponent;

	EFirstPersonCameraMoveState AppliedMoveState = EFirstPersonCameraMoveState::Idle;

	bool bHasAppliedMoveState = false;
	bool bWasFalling = false;
	bool bSuppressNextLandingShake = false;

	UPROPERTY(Transient)
	TObjectPtr<APlayerCameraManager> CurrentPlayerCameraManager;

	TSubclassOf<UCameraShakeBase> CurrentShakeClass;
};
