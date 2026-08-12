#pragma once

#include "CoreMinimal.h"
#include "Towel/Presentation/TowelQuantityVisualComponent.h"
#include "TowelStackVisualComponent.generated.h"

UCLASS(ClassGroup = (Towel), meta = (BlueprintSpawnableComponent))
class BATHHOUSESIM_API UTowelStackVisualComponent : public UTowelQuantityVisualComponent
{
	GENERATED_BODY()

protected:
	virtual FTransform BuildLocalTransform(int32 VisualIndex) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Stack")
	FVector BaseLocalOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation|Stack", meta = (ClampMin = "0.0"))
	float ZSpacing = 4.0f;

private:
	friend class FBathhouseTowelPresentationTest;
};
