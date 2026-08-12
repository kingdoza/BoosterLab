#pragma once

#include "CoreMinimal.h"
#include "Facility/BathhouseFacilityActor.h"
#include "Interaction/PlayerInteractable.h"
#include "UsedTowelBinActor.generated.h"

class AWorldUsedTowelActor;
class UTowelInventoryComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API AUsedTowelBinActor : public ABathhouseFacilityActor, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	AUsedTowelBinActor();
	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;
	virtual FPlayerInteractionResult ExecuteSecondaryInteraction(const FPlayerInteractionContext& Context) override;

	UFUNCTION(BlueprintPure, Category = "Towel")
	UTowelInventoryComponent* GetInventory() const { return Inventory; }

	bool TryStageOverflowTowel(AWorldUsedTowelActor*& OutTowel);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel")
	TObjectPtr<UTowelInventoryComponent> Inventory;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Overflow", meta = (ClampMin = "0.0"))
	float OverflowMinRadius = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Overflow", meta = (ClampMin = "0.0"))
	float OverflowMaxRadius = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Overflow", meta = (ClampMin = "1.0"))
	float FloorTraceDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Overflow", meta = (ClampMin = "1"))
	int32 PlacementAttempts = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Overflow", meta = (ClampMin = "0.0"))
	float TowelSpacing = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Overflow", meta = (ClampMin = "0.0"))
	float PawnClearance = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Overflow")
	TEnumAsByte<ECollisionChannel> FloorTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Overflow")
	TSubclassOf<AWorldUsedTowelActor> WorldUsedTowelClass;

private:
	FPlayerInteractionResult TransferToHeldBasket(
		const FPlayerInteractionContext& Context,
		int32 RequestedCount,
		EPlayerInteractionIntent Intent);
};
