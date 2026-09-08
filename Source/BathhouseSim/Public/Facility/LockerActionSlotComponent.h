#pragma once

#include "CoreMinimal.h"
#include "Facility/BathhouseFacilitySlotComponent.h"
#include "LockerActionSlotComponent.generated.h"

UCLASS(ClassGroup = (Bathhouse), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API ULockerActionSlotComponent : public UBathhouseFacilitySlotComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Locker")
	FName GetLockerSlotId() const { return LockerSlotId; }

	bool HasStableLockerSlotId() const { return !LockerSlotId.IsNone(); }

protected:
	friend class FBathhouseFacilityPlacementMathTest;
	friend class FBathhouseFacilityPlacementRuntimeTest;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locker")
	FName LockerSlotId;
};
