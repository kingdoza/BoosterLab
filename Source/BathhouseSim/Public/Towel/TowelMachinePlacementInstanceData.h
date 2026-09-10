#pragma once

#include "CoreMinimal.h"
#include "Placement/FacilityPlacementPayload.h"
#include "Towel/TowelTypes.h"
#include "TowelMachinePlacementInstanceData.generated.h"

UCLASS(Transient, NotBlueprintable)
class BATHHOUSESIM_API UTowelMachinePlacementInstanceData final
	: public UFacilityPlacementInstanceData
{
	GENERATED_BODY()

public:
	ETowelMachineKind MachineKind = ETowelMachineKind::Washer;
	float ProcessingDurationSeconds = 10.0f;
};
