#pragma once

#include "CoreMinimal.h"
#include "Towel/TowelTypes.h"
#include "TowelVisualTypes.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

USTRUCT(BlueprintType)
struct BATHHOUSESIM_API FTowelStateMeshVariants
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation")
	ETowelState State = ETowelState::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Towel|Presentation")
	TArray<TObjectPtr<UStaticMesh>> MeshVariants;
};

USTRUCT()
struct FTowelVisualLayerRecord
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Bucket = nullptr;

	UPROPERTY(Transient)
	int32 InstanceIndex = INDEX_NONE;

	UPROPERTY(Transient)
	FTransform LocalTransform = FTransform::Identity;
};
