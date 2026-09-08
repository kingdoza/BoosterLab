#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FacilityPlacementDefinition.generated.h"

class AFacilityPlacementPreviewActor;

UCLASS(BlueprintType)
class BATHHOUSESIM_API UFacilityPlacementDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	FName StableId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
	FGameplayTagContainer FacilityTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
	TSubclassOf<AFacilityPlacementPreviewActor> PreviewActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1", UIMin = "1"))
	int32 FootprintCellsX = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "1", UIMin = "1"))
	int32 FootprintCellsY = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locker", meta = (ClampMin = "0", UIMin = "0"))
	int32 LockerSlotCount = 0;
};
