#pragma once

#include "CoreMinimal.h"
#include "Towel/Presentation/TowelQuantityVisualComponent.h"
#include "TowelStackVisualComponent.generated.h"

UCLASS(ClassGroup = (Towel), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UTowelStackVisualComponent : public UTowelQuantityVisualComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Towel|Presentation|Stack")
	void RebuildPreview();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Towel|Presentation|Stack")
	void ClearPreview();

protected:
	virtual FTransform BuildLocalTransform(int32 VisualIndex) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Stack")
	FVector BaseLocalOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Stack", meta = (ClampMin = "0.0"))
	float ZSpacing = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Stack")
	ETowelState PreviewState = ETowelState::Clean;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Stack", meta = (ClampMin = "0"))
	int32 PreviewCount = 0;

private:
	friend class FBathhouseTowelPresentationTest;

	int64 PreviewRevision = 0;
};
