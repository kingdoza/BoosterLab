// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FirstPersonController.generated.h"

class UInputMappingContext;

UCLASS()
class BOOSTERLAB_API AFirstPersonController : public APlayerController
{
	GENERATED_BODY()

public:
	AFirstPersonController();

protected:
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};
