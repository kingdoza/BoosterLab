#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Placement/FacilityPlacementTypes.h"
#include "PlaceableFacility.generated.h"

class AFacilityPlacementZoneActor;
class UFacilityPlacementComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPlaceableFacility : public UInterface
{
	GENERATED_BODY()
};

class BATHHOUSESIM_API IPlaceableFacility
{
	GENERATED_BODY()

public:
	virtual UFacilityPlacementComponent* GetFacilityPlacementComponent() const = 0;
	virtual FFacilityPlacementTransactionResult QueryFacilityPlacement(
		const FTransform& CandidateTransform,
		const AFacilityPlacementZoneActor& Zone) const = 0;
	virtual FFacilityPlacementTransactionResult QueryFacilityRecovery() const = 0;
	virtual bool CommitPlaceableFacilityMode(EPlaceableFacilityMode NewMode, FText& OutFailureReason) = 0;
};
