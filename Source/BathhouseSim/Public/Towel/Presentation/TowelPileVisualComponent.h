#pragma once

#include "CoreMinimal.h"
#include "Towel/Presentation/TowelQuantityVisualComponent.h"
#include "TowelPileVisualComponent.generated.h"

UCLASS(ClassGroup = (Towel), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UTowelPileVisualComponent : public UTowelQuantityVisualComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Towel|Presentation|Pile")
	void RebuildPreview();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Towel|Presentation|Pile")
	void ClearPreview();

protected:
	virtual FTransform BuildLocalTransform(int32 VisualIndex) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Pile", meta = (ClampMin = "0.0"))
	FVector PileHalfExtent = FVector(35.0f, 35.0f, 25.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Pile")
	FVector BaseLocalOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Pile", meta = (ClampMin = "1"))
	int32 ItemsPerLayer = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Pile", meta = (ClampMin = "0.0"))
	float LayerSpacing = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Pile", meta = (ClampMin = "0.0"))
	float MaxZJitter = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Pile")
	FRotator MinRandomRotation = FRotator(-5.0f, -180.0f, -5.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Pile")
	FRotator MaxRandomRotation = FRotator(5.0f, 180.0f, 5.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Pile")
	ETowelState PreviewState = ETowelState::Clean;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Pile", meta = (ClampMin = "0"))
	int32 PreviewCount = 0;

private:
	friend class FBathhouseTowelPresentationTest;

	int64 PreviewRevision = 0;
};
