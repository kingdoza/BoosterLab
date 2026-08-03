#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interaction/InteractionTypes.h"
#include "PlayerInteractable.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UPlayerInteractable : public UInterface
{
	GENERATED_BODY()
};

class BATHHOUSESIM_API IPlayerInteractable
{
	GENERATED_BODY()

public:
	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const = 0;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) = 0;
};
