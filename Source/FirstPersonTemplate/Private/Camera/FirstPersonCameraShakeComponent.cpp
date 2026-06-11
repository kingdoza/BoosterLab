// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/FirstPersonCameraShakeComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/FirstPersonCharacter.h"
#include "Character/FirstPersonMovementComponent.h"
#include "GameFramework/PlayerController.h"

UFirstPersonCameraShakeComponent::UFirstPersonCameraShakeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;
}

void UFirstPersonCameraShakeComponent::SuppressNextLandingShake()
{
	bSuppressNextLandingShake = true;
}

void UFirstPersonCameraShakeComponent::StopAllCameraShakes()
{
	APlayerController* LocalPlayerController = GetLocalPlayerController();
	if (!LocalPlayerController || !LocalPlayerController->PlayerCameraManager)
	{
		StopCurrentShake();
		return;
	}

	LocalPlayerController->PlayerCameraManager->StopAllCameraShakes(true);
	CurrentPlayerCameraManager = nullptr;
	CurrentShakeClass = nullptr;
	bHasAppliedMoveState = false;
}

void UFirstPersonCameraShakeComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<AFirstPersonCharacter>(GetOwner());
	if (OwnerCharacter)
	{
		OwnerMovementComponent = Cast<UFirstPersonMovementComponent>(OwnerCharacter->GetCharacterMovement());
	}

	bWasFalling = IsOwnerFalling();

	if (ShouldDisableTickForNonLocal())
	{
		SetComponentTickInterval(0.25f);
		return;
	}

	SetComponentTickInterval(0.0f);

	if (!bWasFalling)
	{
		ApplyMoveState(CalculateMoveState());
	}
}

void UFirstPersonCameraShakeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCurrentShake();
	Super::EndPlay(EndPlayReason);
}

void UFirstPersonCameraShakeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ShouldDisableTickForNonLocal())
	{
		StopCurrentShake();
		SetComponentTickInterval(0.25f);
		return;
	}

	SetComponentTickInterval(0.0f);

	const bool bIsFalling = IsOwnerFalling();
	if (bIsFalling)
	{
		StopMoveShake();
		bWasFalling = true;
		return;
	}

	const bool bHasLandedThisFrame = bWasFalling && !bIsFalling;
	if (bHasLandedThisFrame)
	{
		if (bSuppressNextLandingShake)
		{
			bSuppressNextLandingShake = false;
		}
		else
		{
			PlayLandingShake();
		}
	}

	const EFirstPersonCameraMoveState NewState = CalculateMoveState();
	if (!bHasAppliedMoveState || NewState != AppliedMoveState)
	{
		ApplyMoveState(NewState);
	}

	bWasFalling = bIsFalling;
}

EFirstPersonCameraMoveState UFirstPersonCameraShakeComponent::CalculateMoveState() const
{
	if (!OwnerCharacter)
	{
		return EFirstPersonCameraMoveState::Idle;
	}

	const FVector Velocity = OwnerCharacter->GetVelocity();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);
	const float ThresholdSq = FMath::Square(IdleSpeedThreshold);
	if (HorizontalVelocity.SizeSquared() <= ThresholdSq)
	{
		return EFirstPersonCameraMoveState::Idle;
	}

	if (OwnerMovementComponent && OwnerMovementComponent->IsSprinting())
	{
		return EFirstPersonCameraMoveState::Sprint;
	}

	return EFirstPersonCameraMoveState::Walk;
}

void UFirstPersonCameraShakeComponent::ApplyMoveState(EFirstPersonCameraMoveState NewState)
{
	StopMoveShake();

	APlayerController* LocalPlayerController = GetLocalPlayerController();
	if (!LocalPlayerController || !LocalPlayerController->PlayerCameraManager)
	{
		return;
	}

	TSubclassOf<UCameraShakeBase> ShakeClass = GetShakeClassForState(NewState);
	AppliedMoveState = NewState;
	bHasAppliedMoveState = true;
	if (!ShakeClass)
	{
		return;
	}

	CurrentPlayerCameraManager = LocalPlayerController->PlayerCameraManager;
	CurrentPlayerCameraManager->StartCameraShake(ShakeClass);
	CurrentShakeClass = ShakeClass;
}

void UFirstPersonCameraShakeComponent::StopCurrentShake()
{
	StopMoveShake();
}

void UFirstPersonCameraShakeComponent::StopMoveShake()
{
	if (CurrentShakeClass && CurrentPlayerCameraManager)
	{
		CurrentPlayerCameraManager->StopAllInstancesOfCameraShake(CurrentShakeClass, true);
	}

	CurrentPlayerCameraManager = nullptr;
	CurrentShakeClass = nullptr;
	bHasAppliedMoveState = false;
}

void UFirstPersonCameraShakeComponent::PlayLandingShake()
{
	APlayerController* LocalPlayerController = GetLocalPlayerController();
	if (!LocalPlayerController || !LocalPlayerController->PlayerCameraManager || !LandingCameraShakeClass)
	{
		return;
	}

	LocalPlayerController->PlayerCameraManager->StartCameraShake(LandingCameraShakeClass);
}

TSubclassOf<UCameraShakeBase> UFirstPersonCameraShakeComponent::GetShakeClassForState(EFirstPersonCameraMoveState State) const
{
	switch (State)
	{
	case EFirstPersonCameraMoveState::Idle:
		return IdleCameraShakeClass;
	case EFirstPersonCameraMoveState::Walk:
		return WalkCameraShakeClass;
	case EFirstPersonCameraMoveState::Sprint:
		return SprintCameraShakeClass;
	default:
		return nullptr;
	}
}

APlayerController* UFirstPersonCameraShakeComponent::GetLocalPlayerController() const
{
	if (!OwnerCharacter)
	{
		return nullptr;
	}

	APlayerController* PlayerController = Cast<APlayerController>(OwnerCharacter->GetController());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	return PlayerController;
}

bool UFirstPersonCameraShakeComponent::ShouldDisableTickForNonLocal() const
{
	if (!OwnerCharacter)
	{
		return false;
	}

	return OwnerCharacter->GetController() != nullptr && !OwnerCharacter->IsLocallyControlled();
}

bool UFirstPersonCameraShakeComponent::IsOwnerFalling() const
{
	if (!OwnerMovementComponent)
	{
		return false;
	}

	return OwnerMovementComponent->IsFalling();
}
