#pragma once

#include "CoreMinimal.h"
#include "FacilityPlacementPayload.generated.h"

class APlaceableFacilityItemActor;
class UFacilityPlacementDefinition;

UCLASS(Abstract, Transient, NotBlueprintable)
class BATHHOUSESIM_API UFacilityPlacementInstanceData : public UObject
{
	GENERATED_BODY()
};

USTRUCT()
struct BATHHOUSESIM_API FFacilityPlacementPayload
{
	GENERATED_BODY()

	bool Validate(const APlaceableFacilityItemActor& ExpectedOuter, FText& OutFailureReason) const;
	void Reset();

	UPROPERTY(Transient)
	TObjectPtr<UFacilityPlacementDefinition> Definition = nullptr;

	UPROPERTY(Transient, Instanced)
	TObjectPtr<UFacilityPlacementInstanceData> InstanceData = nullptr;
};
