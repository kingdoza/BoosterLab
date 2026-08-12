#pragma once

#include "CoreMinimal.h"
#include "BathhouseFacilityTypes.generated.h"

UENUM(BlueprintType)
enum class EBathhouseFacilityType : uint8
{
	ShoeLocker,
	ClothesLocker,
	Shower,
	Bath,
	DryingSpot,
	TowelBasket,
	Exit,
	TowelShelf
};

UENUM(BlueprintType)
enum class EBathhouseFacilitySlotState : uint8
{
	Available,
	Reserved,
	Occupied
};

UENUM(BlueprintType)
enum class EBathhouseCounterLane : uint8
{
	None,
	CheckIn,
	Checkout
};
