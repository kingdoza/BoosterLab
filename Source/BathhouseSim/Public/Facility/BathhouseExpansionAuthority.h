#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BathhouseExpansionAuthority.generated.h"

class UBathhouseExpansionDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBathhouseExpansionTierChanged, int32, NewTier);

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseExpansionAuthority : public AActor
{
	GENERATED_BODY()

public:
	ABathhouseExpansionAuthority();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Expansion")
	int32 GetCurrentTierIndex() const { return CurrentTierIndex; }

	UFUNCTION(BlueprintPure, Category = "Expansion")
	int32 GetCurrentKeyPoolSize() const;

	UFUNCTION(BlueprintPure, Category = "Expansion")
	int32 GetCurrentMaxInstalledLockerSlots() const;

	UFUNCTION(BlueprintCallable, Category = "Expansion")
	bool TryAdvanceToTier(int32 NewTierIndex, FText& OutFailureReason);

	UPROPERTY(BlueprintAssignable, Category = "Expansion")
	FOnBathhouseExpansionTierChanged OnExpansionTierChanged;

protected:
	friend class FBathhouseFacilityPlacementRuntimeTest;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Expansion")
	TObjectPtr<UBathhouseExpansionDefinition> ExpansionDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Expansion", meta = (ClampMin = "0"))
	int32 InitialTierIndex = 0;

private:
	int32 CurrentTierIndex = INDEX_NONE;
	bool bRegistered = false;
};
