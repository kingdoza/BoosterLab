#pragma once

#include "CoreMinimal.h"
#include "Facility/BathhouseFacilityTypes.h"
#include "Placement/FacilityPlacementPayload.h"
#include "BathhouseFacilityPlacementInstanceData.generated.h"

UCLASS(Transient, NotBlueprintable)
class BATHHOUSESIM_API UBathhouseFacilityPlacementInstanceData final
	: public UFacilityPlacementInstanceData
{
	GENERATED_BODY()

public:
	EBathhouseFacilityType FacilityType = EBathhouseFacilityType::Bath;
	int32 FacilityNumber = INDEX_NONE;
	float SelectionWeight = 1.0f;
	bool bEnabled = true;
};
