#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BathhouseExpansionDefinition.generated.h"

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FBathhouseExpansionTier
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expansion", meta = (ClampMin = "0"))
	int32 KeyPoolSize = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expansion", meta = (ClampMin = "0"))
	int32 MaxInstalledLockerSlots = 0;
};

UCLASS(BlueprintType)
class BATHHOUSESIM_API UBathhouseExpansionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	const FBathhouseExpansionTier* GetTier(int32 TierIndex) const { return Tiers.IsValidIndex(TierIndex) ? &Tiers[TierIndex] : nullptr; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Expansion")
	TArray<FBathhouseExpansionTier> Tiers;
};
