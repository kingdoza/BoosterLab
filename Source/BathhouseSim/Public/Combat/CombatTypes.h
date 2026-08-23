#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FCombatDamageContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> InstigatorActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	TObjectPtr<AActor> CauserActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float Damage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector CameraOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	FVector CameraDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.0"))
	float ImpulseStrength = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	float VerticalImpulse = 0.0f;
};
