#pragma once

#include "CoreMinimal.h"
#include "CleaningTypes.generated.h"

UENUM(BlueprintType)
enum class ECleaningStainType : uint8
{
	Water
};

UENUM(BlueprintType)
enum class EStainCleaningState : uint8
{
	Idle,
	Cleaning,
	Removed
};

UENUM(BlueprintType)
enum class EStainSpawnZoneKind : uint8
{
	BathFloor,
	DressingFloor
};
