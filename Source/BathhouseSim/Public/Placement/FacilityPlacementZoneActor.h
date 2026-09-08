#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "FacilityPlacementZoneActor.generated.h"

class UBoxComponent;
class UFacilityPlacementDefinition;

UCLASS(Blueprintable)
class BATHHOUSESIM_API AFacilityPlacementZoneActor : public AActor
{
	GENERATED_BODY()

public:
	AFacilityPlacementZoneActor();

	bool IsDefinitionAllowed(const UFacilityPlacementDefinition& Definition) const;
	FTransform MakeCandidateTransform(const FVector& WorldPoint, float YawDegrees, bool bSnap) const;
	bool ContainsFootprint(const FTransform& CandidateTransform, const FVector& WorldHalfExtent) const;
	UBoxComponent* GetZoneBounds() const { return ZoneBounds; }

	static float QuantizeLocalCoordinate(float Value, float GridSize);
	static float NormalizePlacementYaw(float YawDegrees);

	UFUNCTION(BlueprintImplementableEvent, Category = "Facility Placement|Presentation")
	void OnGridVisibilityChanged(bool bVisible);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	TObjectPtr<UBoxComponent> ZoneBounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	FGameplayTagContainer AllowedFacilityTags;
};
