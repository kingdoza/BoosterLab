#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Towel/Presentation/TowelVisualTypes.h"
#include "TowelQuantityVisualComponent.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UTowelInventoryComponent;
class UTowelVisualMeshProfile;

UCLASS(Abstract, ClassGroup = (Towel), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UTowelQuantityVisualComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTowelQuantityVisualComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Towel|Presentation")
	void BindInventorySource(UTowelInventoryComponent* InventorySource);

	UFUNCTION(BlueprintCallable, Category = "Towel|Presentation")
	void UnbindInventorySource();

	UFUNCTION(BlueprintCallable, Category = "Towel|Presentation")
	void SetTargetPresentation(ETowelState State, int32 Count, int64 Revision, bool bAnimate = true);

	UFUNCTION(BlueprintCallable, Category = "Towel|Presentation")
	void SynchronizeImmediately();

	UFUNCTION(BlueprintPure, Category = "Towel|Presentation")
	ETowelState GetTargetState() const { return TargetState; }

	UFUNCTION(BlueprintPure, Category = "Towel|Presentation")
	int32 GetTargetCount() const { return TargetCount; }

	UFUNCTION(BlueprintPure, Category = "Towel|Presentation")
	ETowelState GetDisplayedState() const { return DisplayedState; }

	UFUNCTION(BlueprintPure, Category = "Towel|Presentation")
	int32 GetDisplayedCount() const { return DisplayedCount; }

	UFUNCTION(BlueprintPure, Category = "Towel|Presentation")
	int64 GetAppliedRevision() const { return AppliedRevision; }

	UFUNCTION(BlueprintPure, Category = "Towel|Presentation")
	UTowelInventoryComponent* GetBoundInventorySource() const { return BoundInventory.Get(); }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual FTransform BuildLocalTransform(int32 VisualIndex);
	virtual int32 GetVisualCapacity() const;
	virtual void PrepareLayout();

	void RebuildVisibleMeshesPreservingTransforms();
	void ClearPresentation();
	FRandomStream& GetVisualRandomStream() { return RandomStream; }
	const TArray<FTowelVisualLayerRecord>& GetLayerRecords() const { return LayerRecords; }
	const TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>>& GetMeshBuckets() const
	{
		return MeshBuckets;
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation")
	TObjectPtr<UTowelVisualMeshProfile> MeshProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation")
	int32 RandomSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation", meta = (ClampMin = "0.0"))
	float CountStepInterval = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation")
	bool bAnimateInventoryChanges = true;

private:
	friend class FBathhouseTowelPresentationTest;

	UFUNCTION()
	void HandleInventoryChanged(
		const FTowelInventorySnapshot& Previous,
		const FTowelInventorySnapshot& Current,
		int64 TransactionId);

	void ResetForNewSource();
	void EnsureStepTimer();
	void StopStepTimer();
	void AdvanceOneStep();
	void ApplyTargetStateToVisibleLayers();
	int32 GetEffectiveTargetCount() const;
	void AddVisualLayer();
	void RemoveLastVisualLayer();
	UInstancedStaticMeshComponent* FindOrCreateBucket(UStaticMesh* Mesh);
	void DestroyBucket(UStaticMesh* Mesh, UInstancedStaticMeshComponent* Bucket);
	void DestroyAllBuckets();

	UPROPERTY(Transient)
	TWeakObjectPtr<UTowelInventoryComponent> BoundInventory;

	UPROPERTY(Transient)
	TWeakObjectPtr<UTowelInventoryComponent> PendingReregisterInventory;

	UPROPERTY(Transient)
	TArray<FTowelVisualLayerRecord> LayerRecords;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UStaticMesh>, TObjectPtr<UInstancedStaticMeshComponent>> MeshBuckets;

	ETowelState TargetState = ETowelState::None;
	ETowelState DisplayedState = ETowelState::None;
	int32 TargetCount = 0;
	int32 DisplayedCount = 0;
	int64 AppliedRevision = -1;
	FRandomStream RandomStream;
	FTimerHandle StepTimerHandle;
	bool bCleaningUp = false;
};
