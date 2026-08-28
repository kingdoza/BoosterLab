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

UENUM(BlueprintType)
enum class EBathhouseQueueAssignmentType : uint8
{
	Invalid,
	ServicePoint,
	QueuePoint,
	OverflowWander
};

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FBathhouseQueueAssignment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Bathhouse Queue")
	EBathhouseQueueAssignmentType Type = EBathhouseQueueAssignmentType::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Bathhouse Queue")
	FTransform TargetTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "Bathhouse Queue")
	int32 LogicalIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Bathhouse Queue")
	int32 QueuePointIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Bathhouse Queue", meta = (ClampMin = "1"))
	int64 LaneRevision = 1;

	bool IsValid() const { return Type != EBathhouseQueueAssignmentType::Invalid; }
	bool IsVisibleAssignment() const
	{
		return Type == EBathhouseQueueAssignmentType::ServicePoint
			|| Type == EBathhouseQueueAssignmentType::QueuePoint;
	}
};
