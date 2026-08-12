#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/PlayerInteractable.h"
#include "WorldUsedTowelActor.generated.h"

class AUsedTowelBinActor;
class UStaticMeshComponent;
class UTowelInventoryComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API AWorldUsedTowelActor : public AActor, public IPlayerInteractable
{
	GENERATED_BODY()

public:
	AWorldUsedTowelActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FPlayerInteractionQuery QueryInteraction(const FPlayerInteractionContext& Context) const override;
	virtual FPlayerInteractionResult ExecuteInteraction(const FPlayerInteractionContext& Context) override;

	void SetPreferredBin(AUsedTowelBinActor* InBin) { PreferredBin = InBin; }
	void CommitStagedToken();
	bool IsConsumed() const { return bConsumed; }
	bool IsTokenCommitted() const { return bTokenCommitted; }

	UFUNCTION(BlueprintPure, Category = "Towel")
	UTowelInventoryComponent* GetInventory() const { return Inventory; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel")
	TObjectPtr<UStaticMeshComponent> WorldMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Towel")
	TObjectPtr<UTowelInventoryComponent> Inventory;

private:
	UPROPERTY(Transient)
	TObjectPtr<AUsedTowelBinActor> PreferredBin = nullptr;

	bool bConsumed = false;
	bool bTokenCommitted = false;
	bool bRecoveryCommitted = false;
};
