#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FacilityPlacementPreviewActor.generated.h"

class USceneComponent;

UCLASS(Blueprintable, NotPlaceable)
class BATHHOUSESIM_API AFacilityPlacementPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AFacilityPlacementPreviewActor();
	void SetPlacementValidity(bool bValid, const FText& FailureReason);

	UFUNCTION(BlueprintImplementableEvent, Category = "Facility Placement|Presentation")
	void OnPlacementValidityChanged(bool bValid, const FText& FailureReason);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	TObjectPtr<USceneComponent> SceneRoot;
};
