// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/FirstPersonController.h"

#include "Camera/FirstPersonCameraManager.h"
#include "EnhancedInputSubsystems.h"

AFirstPersonController::AFirstPersonController()
{
	PlayerCameraManagerClass = AFirstPersonCameraManager::StaticClass();
}

void AFirstPersonController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!DefaultMappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void AFirstPersonController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DefaultMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(DefaultMappingContext);
		}
	}

	Super::EndPlay(EndPlayReason);
}
