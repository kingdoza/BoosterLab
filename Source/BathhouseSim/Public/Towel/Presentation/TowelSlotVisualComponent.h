#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Towel/Presentation/TowelQuantityVisualComponent.h"
#include "TowelSlotVisualComponent.generated.h"

class USceneComponent;

UCLASS(ClassGroup = (Towel), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UTowelSlotVisualComponent : public UTowelQuantityVisualComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Towel|Presentation|Slot")
	void RebuildPreview();

	UFUNCTION(BlueprintPure, Category = "Towel|Presentation|Slot")
	int32 GetValidSlotCount() const { return ResolvedSlots.Num(); }

protected:
	virtual void OnRegister() override;
	virtual FTransform BuildLocalTransform(int32 VisualIndex) override;
	virtual int32 GetVisualCapacity() const override;
	virtual void PrepareLayout() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Slot")
	TArray<FComponentReference> SlotReferences;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Slot")
	ETowelState PreviewState = ETowelState::Clean;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Slot", meta = (ClampMin = "0"))
	int32 PreviewCount = 0;

private:
	friend class FBathhouseTowelPresentationTest;

	void ResolveSlots();

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> ResolvedSlots;

	int64 PreviewRevision = 0;
	int32 LastWarnedOverflowCount = INDEX_NONE;
};
