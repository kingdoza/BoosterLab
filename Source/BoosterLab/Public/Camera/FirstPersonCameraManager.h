// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "FirstPersonCameraManager.generated.h"

UCLASS(Blueprintable)
class BOOSTERLAB_API AFirstPersonCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	AFirstPersonCameraManager();
};
