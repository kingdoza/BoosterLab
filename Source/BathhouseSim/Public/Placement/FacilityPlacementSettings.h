#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FacilityPlacementSettings.generated.h"

class UMaterialInterface;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Facility Placement"))
class BATHHOUSESIM_API UFacilityPlacementSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFacilityPlacementSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	float GetGridSizeCm() const { return FMath::Max(1.0f, GridSizeCm); }
	float GetRotationStepDegrees() const { return FMath::Clamp(RotationStepDegrees, 1.0f, 180.0f); }
	float GetRecoveryHoldSeconds() const { return FMath::Max(0.1f, RecoveryHoldSeconds); }
	float GetRecoveryDropZOffsetCm() const { return FMath::Max(0.0f, RecoveryDropZOffsetCm); }
	float GetPlacementTraceDistance() const { return FMath::Max(1.0f, PlacementTraceDistance); }
	float GetRecoveryTraceDistance() const { return FMath::Max(1.0f, RecoveryTraceDistance); }
	FTransform GetFacilityItemHeldTransform() const
	{
		FTransform Result = FacilityItemHeldTransform;
		Result.SetScale3D(FVector::OneVector);
		return Result;
	}
	UMaterialInterface* LoadValidPreviewMaterial() const
	{
		return ValidPreviewMaterial.IsValid() ? ValidPreviewMaterial.Get() : ValidPreviewMaterial.LoadSynchronous();
	}
	UMaterialInterface* LoadInvalidPreviewMaterial() const
	{
		return InvalidPreviewMaterial.IsValid() ? InvalidPreviewMaterial.Get() : InvalidPreviewMaterial.LoadSynchronous();
	}

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float GridSizeCm = 10.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1.0", ClampMax = "180.0", UIMin = "1.0", UIMax = "180.0", ForceUnits = "deg"))
	float RotationStepDegrees = 15.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.1", UIMin = "0.1", ForceUnits = "s"))
	float RecoveryHoldSeconds = 1.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Recovery", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float RecoveryDropZOffsetCm = 100.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float PlacementTraceDistance = 500.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float RecoveryTraceDistance = 300.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Carry")
	FTransform FacilityItemHeldTransform = FTransform::Identity;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Preview")
	TSoftObjectPtr<UMaterialInterface> ValidPreviewMaterial;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Preview")
	TSoftObjectPtr<UMaterialInterface> InvalidPreviewMaterial;
};
