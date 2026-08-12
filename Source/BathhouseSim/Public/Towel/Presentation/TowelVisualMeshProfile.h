#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Towel/Presentation/TowelVisualTypes.h"
#include "TowelVisualMeshProfile.generated.h"

class UStaticMesh;

UCLASS(BlueprintType)
class BATHHOUSESIM_API UTowelVisualMeshProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UStaticMesh* SelectMesh(ETowelState State, FRandomStream& RandomStream) const;
	const TArray<TObjectPtr<UStaticMesh>>& GetValidVariants(ETowelState State) const;
	bool ValidateProfile(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;

	virtual void PostLoad() override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation")
	TArray<FTowelStateMeshVariants> StateVariants;

private:
	friend class FBathhouseTowelPresentationTest;

	void InvalidateCache();
	void RebuildCache(TArray<FText>* OutErrors = nullptr, TArray<FText>* OutWarnings = nullptr) const;

	mutable bool bCacheValid = false;
	mutable TMap<ETowelState, TArray<TObjectPtr<UStaticMesh>>> CachedVariants;
};
