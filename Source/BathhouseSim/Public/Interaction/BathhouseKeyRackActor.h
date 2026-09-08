#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BathhouseKeyRackActor.generated.h"

class ABathhouseExpansionAuthority;
class ABathhouseKeyActor;
class ABathhouseKeyHookActor;
class USceneComponent;

UCLASS(Blueprintable)
class BATHHOUSESIM_API ABathhouseKeyRackActor : public AActor
{
	GENERATED_BODY()

public:
	ABathhouseKeyRackActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Bathhouse Key Rack")
	int32 GetMaterializedPairCount() const { return OwnedKeys.Num(); }

protected:
	friend class FBathhouseFacilityPlacementRuntimeTest;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bathhouse Key Rack")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Key Rack")
	TArray<FTransform> PairTransforms;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bathhouse Key Rack")
	TSubclassOf<ABathhouseKeyActor> KeyClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bathhouse Key Rack")
	TSubclassOf<ABathhouseKeyHookActor> HookClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bathhouse Key Rack", meta = (ClampMin = "0"))
	int32 FirstKeyNumber = 0;

private:
	UFUNCTION()
	void HandleExpansionTierChanged(int32 NewTier);

	void HandleExpansionAuthorityChanged(ABathhouseExpansionAuthority* Authority);
	void BindAuthority(ABathhouseExpansionAuthority* Authority);
	void MaterializeToPoolSize(int32 PoolSize);

	UPROPERTY(Transient)
	TObjectPtr<ABathhouseExpansionAuthority> BoundAuthority = nullptr;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABathhouseKeyActor>> OwnedKeys;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABathhouseKeyHookActor>> OwnedHooks;

	FDelegateHandle AuthorityRegistryHandle;
};
