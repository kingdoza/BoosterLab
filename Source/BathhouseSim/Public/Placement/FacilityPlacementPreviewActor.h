#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FacilityPlacementPreviewActor.generated.h"

class USceneComponent;
class UMaterialInterface;
class UStaticMeshComponent;

UCLASS(Blueprintable, NotPlaceable)
class BATHHOUSESIM_API AFacilityPlacementPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AFacilityPlacementPreviewActor();
	bool InitializeFromPlacedClass(TSubclassOf<AActor> PlacedClass, FText& OutFailureReason);
	bool ValidateSourceGeometry(TSubclassOf<AActor> PlacedClass, FText& OutFailureReason) const;
	void SetPlacementValidity(bool bValid, const FText& FailureReason);
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetPreviewMeshes() const { return PreviewMeshes; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Facility Placement|Presentation")
	void OnPlacementValidityChanged(bool bValid, const FText& FailureReason);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Placement")
	TObjectPtr<USceneComponent> SceneRoot;

private:
	bool ApplyPreviewMaterial(UMaterialInterface* Material, FText& OutFailureReason);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PreviewMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ValidMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> InvalidMaterial;

	UPROPERTY(Transient)
	TSubclassOf<AActor> SourcePlacedClass;

	FTransform SourceFootprintRelative = FTransform::Identity;
	FVector SourceFootprintExtent = FVector::ZeroVector;
	FVector SourceRootScale = FVector::OneVector;
};
