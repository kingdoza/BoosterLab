#pragma once

#include "CoreMinimal.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Interaction/PlayerInteractable.h"
#include "CleanTowelStackActor.generated.h"

class UTowelInventoryComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ACleanTowelStackActor : public ABathhouseFacilityActor, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	ACleanTowelStackActor();
	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual FPlayerInteractionResult ExecuteSecondaryInteraction(const FPlayerInteractionContext& Context) override;

	UFUNCTION(BlueprintPure, Category = "Towel")
	UTowelInventoryComponent* GetInventory() const { return Inventory; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel")
	TObjectPtr<UTowelInventoryComponent> Inventory;

private:
	FPlayerInteractionResult TransferFromHeldBasket(
		const FPlayerInteractionContext& Context,
		int32 RequestedCount,
		EPlayerInteractionIntent Intent);
};
