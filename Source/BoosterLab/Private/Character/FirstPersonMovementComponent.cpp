// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/FirstPersonMovementComponent.h"

UFirstPersonMovementComponent::UFirstPersonMovementComponent()
{
	MaxWalkSpeed = WalkingSpeed;
}

void UFirstPersonMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxWalkSpeed = bIsSprinting ? SprintingSpeed : WalkingSpeed;
}

void UFirstPersonMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsSprinting && Acceleration.GetSafeNormal().IsNearlyZero())
	{
		StopSprinting();
	}
}

void UFirstPersonMovementComponent::StartSprinting()
{
	if (bIsSprinting || !IsForwardAccelerating() || IsFalling())
	{
		return;
	}

	bIsSprinting = true;
	MaxWalkSpeed = SprintingSpeed;
}

void UFirstPersonMovementComponent::StopSprinting()
{
	if (!bIsSprinting)
	{
		return;
	}

	bIsSprinting = false;
	MaxWalkSpeed = WalkingSpeed;
}

void UFirstPersonMovementComponent::SwitchSprinting()
{
	if (bIsSprinting)
	{
		StopSprinting();
		return;
	}

	StartSprinting();
}

bool UFirstPersonMovementComponent::IsForwardAccelerating() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	const FVector InputDirection = Acceleration.GetSafeNormal2D();
	if (InputDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector OwnerForward = OwnerActor->GetActorForwardVector().GetSafeNormal2D();
	constexpr float ForwardThreshold = 0.01f;
	return FVector::DotProduct(InputDirection, OwnerForward) > ForwardThreshold;
}
