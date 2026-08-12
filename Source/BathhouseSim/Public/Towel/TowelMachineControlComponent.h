#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Interaction/PlayerInteractable.h"
#include "TowelMachineControlComponent.generated.h"

UCLASS(ClassGroup = (Towel), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UTowelMachineControlComponent : public UBoxComponent, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	UTowelMachineControlComponent();
	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
};
