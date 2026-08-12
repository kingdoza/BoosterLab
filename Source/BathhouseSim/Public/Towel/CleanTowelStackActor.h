#pragma once

#include "CoreMinimal.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Interaction/PlayerInteractable.h"
#include "CleanTowelStackActor.generated.h"

class UTowelInventoryComponent;
class UTowelStackVisualComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ACleanTowelStackActor : public ABathhouseFacilityActor, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	ACleanTowelStackActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual FPlayerInteractionResult ExecuteSecondaryInteraction(const FPlayerInteractionContext& Context) override;

	UFUNCTION(BlueprintPure, Category = "Towel")
	UTowelInventoryComponent* GetInventory() const { return Inventory; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel")
	TObjectPtr<UTowelInventoryComponent> Inventory;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel|Presentation")
	TObjectPtr<UTowelStackVisualComponent> TowelPresentationVisual;

private:
	FPlayerInteractionResult TransferFromHeldBasket(
		const FPlayerInteractionContext& Context,
		int32 RequestedCount,
		EPlayerInteractionIntent Intent);
};
