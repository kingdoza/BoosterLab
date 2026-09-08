#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interaction/InteractionTypes.h"
#include "SupplementalInteractionIntentSource.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USupplementalInteractionIntentSource : public UInterface
{
	GENERATED_BODY()
};

/** Adds optional interaction rows without transferring their domain state to Interaction. */
class BATHHOUSESIM_API ISupplementalInteractionIntentSource
{
	GENERATED_BODY()

public:
	virtual FPlayerInteractionQuery MergeSupplementalInteractionQuery(
		const FPlayerInteractionQuery& BaseQuery) const = 0;
};
